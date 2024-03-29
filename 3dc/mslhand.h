#ifndef _included_mslhand_h_
#define _included_mslhand_h_

typedef enum MSL_Section
{
	MSLS_COMPILEDIN,
	MSLS_CHARACTER,
	MSLS_ENVIRONMENT,

	MSLS_MAXSECTIONS

} MSL_SECTION;

#define MSL_OVERFLOW (-1)

int GetNextMSLEntry(MSL_SECTION, unsigned int num_shapes, int auto_delete);

void FlushMSLSection(MSL_SECTION);

#endif /* !_included_mslhand_h_ */
