#include "3dc.h"
#include "3dc.h"
#include "module.h"
#include "inline.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "language.h"
#include "savegame.h"
#include "io.h"

#define MAX_NO_OF_MESSAGES_IN_HISTORY 64

extern void NewOnScreenMessage(char *messagePtr);

typedef struct MessageHistory
{
	enum TEXTSTRING_ID StringID;
	int Hours;
	int Minutes;
	int Seconds;
} MessageHistory;

static MessageHistory MessageHistoryStore[MAX_NO_OF_MESSAGES_IN_HISTORY];
static int NumberOfEntriesInMessageHistory;
static int EntryToNextShow;
static int MessageHistoryAccessedTimer;

void MessageHistory_Initialise(void)
{
	NumberOfEntriesInMessageHistory=0;
	EntryToNextShow=0;
	MessageHistoryAccessedTimer=0;
}

void MessageHistory_Add(enum TEXTSTRING_ID stringID)
{
	if (NumberOfEntriesInMessageHistory<MAX_NO_OF_MESSAGES_IN_HISTORY)
	{
		MessageHistoryStore[NumberOfEntriesInMessageHistory].StringID = stringID;
		MessageHistoryStore[NumberOfEntriesInMessageHistory].Hours = AvP.ElapsedHours;
		MessageHistoryStore[NumberOfEntriesInMessageHistory].Minutes = AvP.ElapsedMinutes;
		MessageHistoryStore[NumberOfEntriesInMessageHistory].Seconds = AvP.ElapsedSeconds/65536;
		NumberOfEntriesInMessageHistory++;
	}
}

void MessageHistory_DisplayPrevious(void)
{
	if (EntryToNextShow) 
	{
		char buffer[1024];

		EntryToNextShow--;
		sprintf
		(
			buffer,
			"%s %d (%02dh%02dm%02ds) \n \n%s",
			GetTextString(TEXTSTRING_INGAME_MESSAGENUMBER),
			EntryToNextShow+1,
			MessageHistoryStore[EntryToNextShow].Hours,
			MessageHistoryStore[EntryToNextShow].Minutes,
			MessageHistoryStore[EntryToNextShow].Seconds,
			GetTextString(MessageHistoryStore[EntryToNextShow].StringID)
		);
		NewOnScreenMessage(buffer);
		MessageHistoryAccessedTimer = 65536*4;

		if (!EntryToNextShow && NumberOfEntriesInMessageHistory) EntryToNextShow = NumberOfEntriesInMessageHistory;
	}
}

void MessageHistory_Maintain(void)
{
	if (MessageHistoryAccessedTimer)
	{
		MessageHistoryAccessedTimer -= NormalFrameTime;

		if (MessageHistoryAccessedTimer<0)
		{
			MessageHistoryAccessedTimer=0;
		}
	}
	else
	{
		EntryToNextShow = NumberOfEntriesInMessageHistory;
	}
}


/*---------------------------**
** Load/Save message history **
**---------------------------*/

typedef struct message_history_save_block
{
	SAVE_BLOCK_HEADER header;

	int NumberOfEntriesInMessageHistory;
	int EntryToNextShow;
	int MessageHistoryAccessedTimer;
	//follow by message history array
}MESSAGE_HISTORY_SAVE_BLOCK;


void Load_MessageHistory(SAVE_BLOCK_HEADER* header)
{
	int i;
	MESSAGE_HISTORY_SAVE_BLOCK* block = (MESSAGE_HISTORY_SAVE_BLOCK*) header;
	MessageHistory* saved_message = (struct MessageHistory*) (block+1);

	int expected_size;

	//make sure the block is the correct size
	expected_size = sizeof(*block);
	expected_size += sizeof(MessageHistory) * block->NumberOfEntriesInMessageHistory;
	if(header->size != expected_size) return;
	
	//load the stuff then
	NumberOfEntriesInMessageHistory = block->NumberOfEntriesInMessageHistory;
	EntryToNextShow = block->EntryToNextShow;
	MessageHistoryAccessedTimer = block->MessageHistoryAccessedTimer;

	//load the message history
	for(i=0;i<block->NumberOfEntriesInMessageHistory;i++)
	{
		MessageHistoryStore[i] = *saved_message++;
	}
}

void Save_MessageHistory()
{
	MESSAGE_HISTORY_SAVE_BLOCK* block;
	int i;

	//get memory for header
	GET_SAVE_BLOCK_POINTER(MESSAGE_HISTORY_SAVE_BLOCK, block);

	//fill in header
	block->header.type = SaveBlock_MessageHistory;
	block->header.size = sizeof(*block) + NumberOfEntriesInMessageHistory * sizeof(MessageHistory);

	block->NumberOfEntriesInMessageHistory = NumberOfEntriesInMessageHistory;
	block->EntryToNextShow = EntryToNextShow;
	block->MessageHistoryAccessedTimer = MessageHistoryAccessedTimer;

	for(i=0;i<NumberOfEntriesInMessageHistory;i++)
	{
		MessageHistory* message = GET_SAVE_BLOCK_POINTER(MessageHistory, message);
		*message = MessageHistoryStore[i];
	}
}
