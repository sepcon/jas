#ifndef __axz_json__
#define __axz_json__

#include "axz_export.h"
#include "axz_dict.h"

namespace AxzJson
{
	AXZDICT_DECLSPEC axz_rc serialize( const AxzDict& in_dict, axz_wstring& out_json, bool in_nice_format = false );
	AXZDICT_DECLSPEC axz_rc deserialize( const axz_wstring& in_json, AxzDict& out_dict );
};

#endif
