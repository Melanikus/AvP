// The Connection check's Windows half: what Windows says about this PC's networks and firewalls (connection_check.h says what and why).
// Written 2026-11-09 by Melanikus for the AvP port.
//
// READ ONLY, AND NO ADMIN. Each of the three things asked is a question any user may ask, through Windows' own documented interfaces:
//   - the Network List Manager (netlistmgr.h): the connected networks' kinds. Never their names;
//   - Windows Firewall's policy (netfw.h, INetFwPolicy2): how it's set for each kind, and its rules, of which only those for this
//     program are looked at;
//   - Windows' Security Center (iwscapi.h, IWSCProductList): the firewalls it knows of, each one's name and whether it's on.
// Nothing is changed, and nothing read leaves the PC: the words go on the screen and one line goes in the log (ConnectionCheck_LogLine).
//
// ON A THREAD OF ITS OWN. Reading a firewall's rules can take a moment, and the menu must never wait, so each read runs on a thread that
// sets up its own COM, keeps what it finds to itself, and hands the whole result over at the end, under a lock. A read is started only
// when the page asks (ConnectionCheck_Start), never when the game starts.
//
// WHAT CAN'T BE KNOWN. Another firewall's own rules (Windows has no way to show them), and whether the network itself blocks anything:
// that's for the list's and the relay's own answers to say.

#include <windows.h>
#include <objbase.h>
#include <oleauto.h>
#include <netfw.h>
#include <netlistmgr.h>
#include <wscapi.h>
#include <iwscapi.h>
#include <shellapi.h>
#include <string.h>
#include <mutex>
#include <string>

#include "connection_check.h"
#include "logString.h"

// COM itself (ole32, linked already) and its strings and values (oleaut32: SysFreeString, VariantClear), both Windows' own
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace
{
	// What the threads share, only ever under the lock: whether a read is going, and the last one's result
	std::mutex &Lock()
	{
		static std::mutex lock;
		return lock;
	}

	bool reading = false;
	bool haveResult = false;
	ConnectionFacts lastResult = {};

	// The most of each thing a read looks at, so a machine with a strange number of networks, rules or products can't keep it going
	const unsigned long kMostNetworks = 64;
	const unsigned long kMostRules = 20000;
	const long kMostProducts = 64;

	template <typename T>
	void Release(T *&thing)
	{
		if (thing)
		{
			thing->Release();
			thing = NULL;
		}
	}

	// A network's category as one of the kinds' bits (connection_check.h)
	unsigned KindOf(NLM_NETWORK_CATEGORY category)
	{
		switch (category)
		{
			case NLM_NETWORK_CATEGORY_PUBLIC:
				return kConnPublic;
			case NLM_NETWORK_CATEGORY_PRIVATE:
				return kConnPrivate;
			case NLM_NETWORK_CATEGORY_DOMAIN_AUTHENTICATED:
				return kConnDomain;
			default:
				return 0;
		}
	}

	// The connected networks: how many, and their kinds
	void ReadNetworks(ConnectionFacts &f)
	{
		INetworkListManager *manager = NULL;
		IEnumNetworks *networks = NULL;

		if (SUCCEEDED(CoCreateInstance(__uuidof(NetworkListManager), NULL, CLSCTX_ALL, __uuidof(INetworkListManager), (void **)&manager)) &&
			SUCCEEDED(manager->GetNetworks(NLM_ENUM_NETWORK_CONNECTED, &networks)) && networks)
		{
			f.networksRead = true;

			for (unsigned long seen = 0; seen < kMostNetworks; seen++)
			{
				INetwork *network = NULL;
				ULONG got = 0;

				if (networks->Next(1, &network, &got) != S_OK || got != 1 || !network)
				{
					Release(network);
					break;
				}

				NLM_NETWORK_CATEGORY category;

				if (SUCCEEDED(network->GetCategory(&category)))
				{
					f.kinds |= KindOf(category);
				}

				f.connected++;
				Release(network);
			}
		}

		Release(networks);
		Release(manager);
	}

	// Whether a rule's program is this one: its path with any %...% filled in, against this program's own path, letter case aside
	bool IsThisProgram(BSTR ruleProgram, const wchar_t *ours)
	{
		if (!ruleProgram || !ours || !ours[0])
		{
			return false;
		}

		wchar_t expanded[MAX_PATH * 4];
		DWORD const length = ExpandEnvironmentStringsW(ruleProgram, expanded, sizeof(expanded) / sizeof(expanded[0]));

		if (length == 0 || length > sizeof(expanded) / sizeof(expanded[0]))
		{
			return false;
		}

		return CompareStringOrdinal(expanded, -1, ours, -1, TRUE) == CSTR_EQUAL;
	}

	// One rule: if it's this program's and switched on, which kinds it stops going out, lets out, or stops coming in
	void ReadRule(INetFwRule *rule, const wchar_t *ours, ConnectionFacts &f)
	{
		BSTR program = NULL;

		if (FAILED(rule->get_ApplicationName(&program)) || !IsThisProgram(program, ours))
		{
			SysFreeString(program);
			return;
		}

		SysFreeString(program);
		VARIANT_BOOL enabled = VARIANT_FALSE;
		NET_FW_RULE_DIRECTION direction = NET_FW_RULE_DIR_IN;
		NET_FW_ACTION action = NET_FW_ACTION_ALLOW;
		long profiles = 0;
		long protocol = 0;

		if (FAILED(rule->get_Enabled(&enabled)) || enabled != VARIANT_TRUE || FAILED(rule->get_Direction(&direction)) ||
			FAILED(rule->get_Action(&action)) || FAILED(rule->get_Profiles(&profiles)) || FAILED(rule->get_Protocol(&protocol)))
		{
			return;
		}

		unsigned const kinds = (unsigned)profiles & kConnAllKinds;

		if (direction == NET_FW_RULE_DIR_OUT)
		{
			// Going out, any protocol counts: the list is reached by TCP, and the relay by UDP
			if (action == NET_FW_ACTION_BLOCK)
			{
				f.avpxOutBlocked |= kinds;
			}
			else if (action == NET_FW_ACTION_ALLOW)
			{
				f.avpxOutAllowed |= kinds;
			}
		}
		else if (direction == NET_FW_RULE_DIR_IN && action == NET_FW_ACTION_BLOCK &&
			(protocol == NET_FW_IP_PROTOCOL_UDP || protocol == NET_FW_IP_PROTOCOL_ANY))
		{
			// Coming in, only UDP matters: a LAN game is UDP
			f.avpxInBlocked |= kinds;
		}
	}

	// Windows Firewall: how it's set for each kind, then its rules for this program
	void ReadFirewall(ConnectionFacts &f)
	{
		INetFwPolicy2 *policy = NULL;

		if (FAILED(CoCreateInstance(__uuidof(NetFwPolicy2), NULL, CLSCTX_INPROC_SERVER, __uuidof(INetFwPolicy2), (void **)&policy)))
		{
			return;
		}

		NET_FW_PROFILE_TYPE2 const kinds[] = {NET_FW_PROFILE2_DOMAIN, NET_FW_PROFILE2_PRIVATE, NET_FW_PROFILE2_PUBLIC};
		bool allRead = true;

		for (size_t i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++)
		{
			VARIANT_BOOL on = VARIANT_FALSE;
			VARIANT_BOOL blockAll = VARIANT_FALSE;
			NET_FW_ACTION out = NET_FW_ACTION_ALLOW;

			if (FAILED(policy->get_FirewallEnabled(kinds[i], &on)) || FAILED(policy->get_DefaultOutboundAction(kinds[i], &out)) ||
				FAILED(policy->get_BlockAllInboundTraffic(kinds[i], &blockAll)))
			{
				allRead = false;
				break;
			}

			unsigned const bit = (unsigned)kinds[i] & kConnAllKinds;
			f.firewallOn |= on == VARIANT_TRUE ? bit : 0;
			f.outBlockedByDefault |= out == NET_FW_ACTION_BLOCK ? bit : 0;
			f.inAllBlocked |= blockAll == VARIANT_TRUE ? bit : 0;
		}

		f.firewallRead = allRead;

		// This program's own path, the one Windows' rules name it by
		wchar_t ours[MAX_PATH * 4];
		DWORD const ourLength = GetModuleFileNameW(NULL, ours, sizeof(ours) / sizeof(ours[0]));
		bool const haveOurs = ourLength > 0 && ourLength < sizeof(ours) / sizeof(ours[0]);

		INetFwRules *rules = NULL;
		IUnknown *all = NULL;
		IEnumVARIANT *each = NULL;

		if (allRead && haveOurs && SUCCEEDED(policy->get_Rules(&rules)) && rules && SUCCEEDED(rules->get__NewEnum(&all)) && all &&
			SUCCEEDED(all->QueryInterface(__uuidof(IEnumVARIANT), (void **)&each)) && each)
		{
			bool ended = false;

			for (unsigned long seen = 0; seen < kMostRules; seen++)
			{
				VARIANT item;
				VariantInit(&item);
				ULONG got = 0;

				if (each->Next(1, &item, &got) != S_OK || got != 1)
				{
					VariantClear(&item);
					ended = true;
					break;
				}

				INetFwRule *rule = NULL;

				if (item.vt == VT_DISPATCH && item.pdispVal && SUCCEEDED(item.pdispVal->QueryInterface(__uuidof(INetFwRule), (void **)&rule)))
				{
					ReadRule(rule, ours, f);
				}

				Release(rule);
				VariantClear(&item);
			}

			// Read only if every rule was looked at: past the most, some rule might not have been
			f.rulesRead = ended;
		}

		Release(each);
		Release(all);
		Release(rules);
		Release(policy);
	}

	// The firewalls Windows' Security Center knows of: how many are on, and the first few by name, made safe
	void ReadOthers(ConnectionFacts &f)
	{
		IWSCProductList *products = NULL;
		LONG count = 0;

		if (FAILED(CoCreateInstance(__uuidof(WSCProductList), NULL, CLSCTX_INPROC_SERVER, __uuidof(IWSCProductList), (void **)&products)) ||
			FAILED(products->Initialize(WSC_SECURITY_PROVIDER_FIREWALL)) || FAILED(products->get_Count(&count)) || count < 0)
		{
			Release(products);
			return;
		}

		f.othersRead = true;

		for (LONG i = 0; i < count && i < kMostProducts; i++)
		{
			IWscProduct *product = NULL;
			WSC_SECURITY_PRODUCT_STATE state = WSC_SECURITY_PRODUCT_STATE_OFF;

			if (FAILED(products->get_Item((ULONG)i, &product)) || !product || FAILED(product->get_ProductState(&state)) ||
				state != WSC_SECURITY_PRODUCT_STATE_ON)
			{
				Release(product);
				continue;
			}

			if (f.othersOn < kConnMostOthers)
			{
				BSTR name = NULL;

				if (SUCCEEDED(product->get_ProductName(&name)) && name)
				{
					ConnectionCheck_SafeName(name, SysStringLen(name), f.others[f.othersOn], sizeof(f.others[f.othersOn]));
				}

				SysFreeString(name);
			}

			f.othersOn++;
			Release(product);
		}

		Release(products);
	}

	DWORD WINAPI ReadThread(LPVOID)
	{
		ConnectionFacts facts = {};
		HRESULT const com = CoInitializeEx(NULL, COINIT_MULTITHREADED);

		if (SUCCEEDED(com))
		{
			ReadNetworks(facts);
			ReadFirewall(facts);
			ReadOthers(facts);
			CoUninitialize();
		}

		char line[kConnLineBytes * 2];
		ConnectionCheck_LogLine(facts, line, sizeof(line));
		LogString(line);

		std::lock_guard<std::mutex> hold(Lock());
		lastResult = facts;
		haveResult = true;
		reading = false;
		return 0;
	}
}

void ConnectionCheck_Start(void)
{
	{
		std::lock_guard<std::mutex> hold(Lock());

		if (reading)
		{
			return;
		}

		reading = true;
	}

	HANDLE const thread = CreateThread(NULL, 0, ReadThread, NULL, 0, NULL);

	if (thread)
	{
		CloseHandle(thread);
		return;
	}

	// No thread: nothing is read, and the page says so
	std::lock_guard<std::mutex> hold(Lock());
	reading = false;
	lastResult = ConnectionFacts();
	haveResult = true;
}

bool ConnectionCheck_Result(ConnectionFacts *facts)
{
	std::lock_guard<std::mutex> hold(Lock());

	if (!haveResult || !facts)
	{
		return false;
	}

	*facts = lastResult;
	return true;
}

bool ConnectionCheck_Reading(void)
{
	std::lock_guard<std::mutex> hold(Lock());
	return reading;
}

// Two fixed addresses, each opened by the handler Windows itself has for it. Nothing anyone typed ever goes into them.
void ConnectionCheck_OpenNetworkSettings(void)
{
	ShellExecuteW(NULL, L"open", L"ms-settings:network-status", NULL, NULL, SW_SHOWNORMAL);
}

void ConnectionCheck_OpenFirewallSettings(void)
{
	ShellExecuteW(NULL, L"open", L"windowsdefender://network", NULL, NULL, SW_SHOWNORMAL);
}
