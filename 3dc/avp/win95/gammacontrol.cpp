#include "3dc.h"
#include "module.h"
#include "inline.h"
#include "gammacontrol.h"

#include "renderer.h"

static int ActualGammaSetting;
int RequestedGammaSetting;

unsigned char GammaValues[256];

void InitialiseGammaSettings(int gamma)
{
	ActualGammaSetting = gamma + 1;
	RequestedGammaSetting = gamma;
	UpdateGammaSettings();
}
#if 0
void UpdateGammaSettings()
{
	//LPDIRECTDRAWGAMMACONTROL handle=NULL; // BJD
	//DDGAMMARAMP gammaValues; // BJD
	D3DGAMMARAMP gammaValues;

	if (RequestedGammaSetting == ActualGammaSetting) {
		return;
	}

	/*
	    lpDDSPrimary->QueryInterface(IID_IDirectDrawGammaControl,(LPVOID*)&handle);
	    if(!handle)
	    {
	        return;
	    }
	*/
	//  handle->GetGammaRamp(0,&gammaValues);
	d3d.lpD3DDevice->GetGammaRamp(0, &gammaValues);

	for (int i = 0; i <= 255; i++) {
		int u = ((i * 65536) / 255);
		int m = MUL_FIXED(u, u);
		int l = MUL_FIXED(2 * u, ONE_FIXED - u);
		int a;
		a = m / 256 + MUL_FIXED(RequestedGammaSetting, l);

		if (a < 0) {
			a = 0;
		}

		if (a > 255) {
			a = 255;
		}

		gammaValues.red[i]   = a * 256;
		gammaValues.green[i] = a * 256;
		gammaValues.blue[i]  = a * 256;
	}

#if ForceCalibration
	d3d.lpD3DDevice->SetGammaRamp(D3DSGR_CALIBRATE, &gammaValues);
#else
	d3d.lpD3DDevice->SetGammaRamp(0, D3DSGR_NO_CALIBRATION, &gammaValues);
#endif
	//HRESULT result = handle->SetGammaRamp(0,&gammaValues);
	//RELEASE(handle);
	ActualGammaSetting = RequestedGammaSetting;
}
#else
void UpdateGammaSettings()
{
	//LPDIRECTDRAWGAMMACONTROL handle=NULL; // BJD
	//DDGAMMARAMP gammaValues; // BJD
	//  D3DGAMMARAMP gammaValues;
	if (RequestedGammaSetting == ActualGammaSetting) {
		return;
	}

	for (int i = 0; i <= 255; i++) {
		int u = ((i * 65536) / 255);
		int m = MUL_FIXED(u, u);
		int l = MUL_FIXED(2 * u, ONE_FIXED - u);
		int a;
		a = m + MUL_FIXED(RequestedGammaSetting * 256, l);
		m = MUL_FIXED(a, a);
		l = MUL_FIXED(2 * a, ONE_FIXED - a);
		a = m / 256 + MUL_FIXED(RequestedGammaSetting, l);

		if (a < 0) {
			a = 0;
		}

		if (a > 255) {
			a = 255;
		}

		GammaValues[i] = a;
	}

	ActualGammaSetting = RequestedGammaSetting;
}
#endif

