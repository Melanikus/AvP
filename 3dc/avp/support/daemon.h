/*
	
	daemon.h

*/

#ifndef _daemon
#define _daemon 1

/* Version settings *****************************************************/
	#define SupportCallbackHooks	FALSE

	#define IndividualTiming		FALSE
		/*
			Should daemons get individually passed a time to run for,
			or do they all share the same timing information?
		*/
/* Constants  ***********************************************************/

/* Macros ***************************************************************/
	#if SupportCallbackHooks
		#define ACTIVITY_RETURN_TYPE	bool

		#define ACTIVITY_RVAL_CHANGE	{return TRUE;}
		#define ACTIVITY_RVAL_NOCHANGE	{return FALSE;}
		#define ACTIVITY_RVAL_BOOL(b)	{return b;}
	#else
		#define ACTIVITY_RETURN_TYPE		void

		#define ACTIVITY_RVAL_CHANGE		{return;}
		#define ACTIVITY_RVAL_NOCHANGE		{return;}
		#define ACTIVITY_RVAL_BOOL(ignore)	{return;}
	#endif

	#if IndividualTiming
		#define ACTIVITY_INPUT			int FixP_Time
	#else
		#define ACTIVITY_INPUT			void
			/* note that int FixP_Time is still available to the activity
			functions, but in the form of a protected member rather than
			an actual parameter
			*/
	#endif

/* Type definitions *****************************************************/
	#ifdef __cplusplus
	class Daemon;

	#if SupportCallbackHooks
	class CallbackHook
	{
		public:
			virtual void OnActivity(void) = 0;

			CallbackHook
			(
				Daemon* p666_New,
				void* pUser_New
			);
			virtual ~CallbackHook();

		// ought to be private:
			CallbackHook* pNxtHook;
			CallbackHook* pPrvHook;
			Daemon* p666_Val; 
			void* pUser_Val;
	};
	#endif // SupportCallbackHooks

	class Daemon
	{
		// Constructors etc:
		public:
			Daemon
			(
				bool fActive
			);

			virtual ~Daemon();

		// Per object stuff:
		public:
			void Start(void);
			void Stop(void);
			void SetActive(bool fActive);
			bool bActive(void) const;

			virtual ACTIVITY_RETURN_TYPE Activity(ACTIVITY_INPUT) = 0;
				// the strategy to run when active; returns TRUE if linked screen objects/gadgets will
				// need updating
			
			#if SupportCallbackHooks
			void ForceHookActivity(void);
				// a way to call the OnActivity() method for all attached hooks
			#endif

		// Static stuff:
		public:
			static Daemon* p666_FirstActive;
			static Daemon* p666_Iteration_Current;
			static Daemon* p666_Iteration_Next;

			static void Maintain(int FixP_Time);

			static int DaemonTimeScale;

		// Private stuff:
		private:
			bool fIsActive_Val;
			Daemon* p666_NextActive;  // only valid if fIsActive
			Daemon* p666_PrevActive;  // only valid if fIsActive

		#if !IndividualTiming
		protected:
			// if all Daemon activity calls share one timing; this is it:
			static int FixP_Time;
		#endif

		#if SupportCallbackHooks
		public: // but probably ought to be private:
			CallbackHook* pFirstHook;
		#endif
	};
	#endif // ifdef __cplusplus

/* Exported globals *****************************************************/

/* Function prototypes **************************************************/
	extern void DAEMON_Init(void);
	extern void DAEMON_Maintain(void);


/* End of the header ****************************************************/

#endif
