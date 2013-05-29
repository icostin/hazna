#include "libpriv.h"

/* hz_lib_name **************************************************************/
HZAPI char const * C41_CALL hzlib_name ()
{
    return "hazna-lemonfresh"
#if _DEBUG
        "-debug"
#else
        "-release"
#endif
        ;
}

/* hze_name *****************************************************************/
#define E(_e) case _e: return #_e
HZAPI char const * C41_CALL hze_name (int e)
{
    switch (e)
    {
    case 0:
        return "HZ_OK";
    default:
        return "*HZE_UNKNOWN*";
    }
}
#undef E

