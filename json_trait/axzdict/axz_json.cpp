#include "axz_json.h"
#include "axz_error_codes.h"
#include "axz_dict_stepper.h"
#include <stack>
#include <algorithm>
#include <cwctype>
#include <string>

namespace
{
namespace Internal
{    
    /*
     * Json stepper class - serialize the dictionary in json format to raw json string
     */
	class AxzJsonStepper: public AxzDictStepper
	{
	public:
		AxzJsonStepper(): AxzDictStepper() {};
		virtual ~AxzJsonStepper() {}
		axz_rc build( const AxzDict& in_dict, axz_wstring& out_json );

		virtual axz_rc step( std::nullptr_t )              override;	
		virtual axz_rc step( const bool val )              override;
		virtual axz_rc step( const int32_t val )           override;
		virtual axz_rc step( const double val )            override;
		virtual axz_rc step( const axz_wstring& val )      override;
		virtual axz_rc step( const axz_bytes& val )        override;
		virtual axz_rc step( const axz_dict_array& vals )  override;
		virtual axz_rc step( const axz_dict_object& vals ) override;

	protected:
		axz_wstring _encodeString( const axz_wstring & to_encode ) const;

	protected:
		axz_wstring m_json;
	};

    /*
     * Nice json stepper class - serialize the dictionary in json format to pretty printed json string
     */
	class AxzNiceJsonStepper final: public AxzJsonStepper
	{
	public:
		AxzNiceJsonStepper(): AxzJsonStepper(), m_indent( 0 ) {};
		virtual ~AxzNiceJsonStepper() {}

		virtual axz_rc step( const axz_dict_array& vals )  override;
		virtual axz_rc step( const axz_dict_object& vals ) override;

	private:
		int m_indent;
	};

	/*
	 * AxzJsonBuilder class - deserialize a string in json format to dictionary type.
     * The work is inspired from 4V WaJsonFactory class. Anything changes in WaJsonFactory needs to be synchronized here
	 */
	namespace AxzJsonBuilder
	{
		axz_rc build( axz_wstring json_in, AxzDict & json_value );
		axz_rc _build( const axz_wstring & str, size_t & pos, AxzDict & json_value );
		axz_rc _buildObject( const axz_wstring & str, size_t & pos, AxzDict & json_object );	
		axz_rc _buildArray( const axz_wstring & str, size_t & pos, AxzDict& json_array );
		axz_rc _buildString( const axz_wstring & str, size_t & pos, axz_wstring & json_string );
		axz_rc _buildSpecialCase( const axz_wstring & str, size_t & pos, AxzDict & json_special );
		axz_rc _buildNumber( const axz_wstring & str, size_t & pos, AxzDict & json_number );
		void _ignoreWhiteSpace( const axz_wstring & str, size_t & pos );
		axz_rc _decodeCharacter( const axz_wstring & json_string, axz_wchar & json_decoded_character, size_t & pos );
		axz_rc _convertToHexQuad( const axz_wstring & str, axz_wchar & converted );
		void _replaceChars( axz_wstring& inout_src, const axz_wstring& in_delimiters, axz_wchar in_replaced );
	};
}
}

namespace AxzJson
{
    axz_rc serialize( const AxzDict& in_dict, axz_wstring& out_json, bool in_nice_format /*= false*/ )
    {
        auto stepper = ( in_nice_format )? ( std::make_shared<Internal::AxzNiceJsonStepper>() ): ( std::make_shared<Internal::AxzJsonStepper>() );
        return stepper->build( in_dict, out_json );
    }
    
	axz_rc deserialize( const axz_wstring& in_json, AxzDict& out_dict )
    {
        return Internal::AxzJsonBuilder::build( in_json, out_dict );
    }
};

namespace
{
namespace Internal
{	
	//-------------< Start - AxzJsonStepper Implementation >----------------------
	//
	axz_rc AxzJsonStepper::build( const AxzDict& in_dict, axz_wstring& out_json )
	{
        out_json.clear();
        this->m_json.clear();

        auto rc = in_dict.step( this->shared_from_this() );
        if ( AXZ_SUCCESS( rc ) ) 
        {
            out_json = std::move( this->m_json );
        }
    
		return rc;
	}

	axz_rc AxzJsonStepper::step( std::nullptr_t )
	{
		this->m_json += L"null";
		return AXZ_OK;
	}

	axz_rc AxzJsonStepper::step( const bool val )
	{
		this->m_json += ( val ? L"true" : L"false" );
		return AXZ_OK;
	}

	axz_rc AxzJsonStepper::step( const int32_t val )
	{
		this->m_json += std::to_wstring( val );	
		return AXZ_OK;
	}

	axz_rc AxzJsonStepper::step( const double val )
	{
		this->m_json += std::to_wstring( val );	
		return AXZ_OK;
	}

	axz_rc AxzJsonStepper::step( const axz_wstring& val )
	{
		this->m_json += L"\"" + this->_encodeString( val ) + L"\"";
		return AXZ_OK;
	}

	axz_rc AxzJsonStepper::step( const axz_bytes& )
	{
		this->m_json += L"\"not supported\"";	// we will encode the bytes with base64 later, not supported for now
		return AXZ_OK;
	}

	axz_rc AxzJsonStepper::step( const axz_dict_array& vals )
	{
		bool first = true;
		this->m_json += L"[";
        axz_rc rc = AXZ_OK;

		for (const auto &item : vals ) 
		{
			if (!first) {
				this->m_json += L", ";
            }
		
			if ( AXZ_FAILED( rc = item.step( this->shared_from_this() ) ) ) {
                break;
            }
			first = false;
		}

        if ( AXZ_SUCCESS( rc ) ) {
    		this->m_json += L"]";
        }
		return rc;
	}

	axz_rc AxzJsonStepper::step( const axz_dict_object& vals )
	{
		bool first = true;
		this->m_json += L"{";
        axz_rc rc = AXZ_OK;

		for ( const auto &kv : vals ) {
			if (!first) {
				this->m_json += L", ";
            }

			this->m_json += L"\"" + this->_encodeString( kv.first ) + L"\": ";				
			if ( AXZ_FAILED( rc = kv.second.step( this->shared_from_this() ) ) ) {
                break;
            }
		
			first = false;
		}
        
        if ( AXZ_SUCCESS( rc ) ) {
    		this->m_json += L"}";
        }

		return rc;
	}

	axz_wstring AxzJsonStepper::_encodeString( const axz_wstring & to_encode ) const
	{
		// store escapes to return
		axz_wstring ret = L"";

		for( size_t i = 0; i < to_encode.size(); ++i )
		{
			// pull character
			axz_wchar c = to_encode[i];

			// set it as if it is our append value already
			axz_wstring append_str = L"";
			append_str += c;

			switch( c )
			{
			case '"':
				append_str = L"\\\"";
				break;
			case '\\':
				append_str = L"\\\\";
				break;
			case '\t':
				append_str = L"\\t";
				break;
			case '\n':
				append_str = L"\\n";
				break;
			case '\r':
				append_str = L"\\r";
				break;
			case '\b':
				append_str = L"\\b";
				break;
			case '\f':
				append_str = L"\\f";
				break;
			default:
				/*	JSON spec:
					"All Unicode characters may be placed within the quotation marks except for the characters that must be
					escaped: quotation mark, reverse solidus, and the control characters (U+0000 through U+001F)."
				*/
				if( c < 0x20 )
				{
					axz_wchar buff[16] = {0};
					std::swprintf( buff, sizeof(buff)/sizeof(*buff), L"\\u%04x", c );
					append_str = buff;
				}
				break;
			}
			ret += append_str;
		}
		return ret;
	}
    //
  	//-------------< End - AxzJsonStepper Implementation >----------------------


   	//-------------< Start - AxzNiceJsonStepper Implementation >----------------------
	//	
	axz_rc AxzNiceJsonStepper::step( const axz_dict_array& vals )
	{			
		bool first = true;
		this->m_json += L"[\n";
		axz_rc rc = AXZ_OK;
		
		for ( const auto &item : vals ) 
		{
			if ( !first ) {
				this->m_json += L",\n";
            }

			this->m_indent += 4;
			for ( int i = 0; i < this->m_indent ; i++)
			{
				 this->m_json += L" ";
			}

			if ( AXZ_FAILED( rc = item.step( this->shared_from_this() ) ) ) {
                break;
            }
			this->m_indent -= 4;
			first = false;
		}	

        if ( AXZ_SUCCESS( rc ) ) {	
		    this->m_json += L"\n";				
		    for ( int i = 0; i < this->m_indent ; i++)
		    {
			    this->m_json += L" ";
		    }
		    this->m_json += L"]";		
        }

		return rc;
	}

	axz_rc AxzNiceJsonStepper::step( const axz_dict_object& vals )
	{		
		bool first = true;
		this->m_json += L"{\n";				
        axz_rc rc = AXZ_OK;

		for ( const auto &kv : vals ) {
			if (!first) {
				this->m_json += L",\n";
            }

			this->m_indent += 4;
			for ( int i = 0; i < this->m_indent ; i++)
			{
				 this->m_json += L" ";
			}
			this->m_json += L"\"" + this->_encodeString( kv.first ) + L"\": ";				

    		if ( AXZ_FAILED( rc = kv.second.step( this->shared_from_this() ) ) ) {
                break;
            }
			this->m_indent -= 4;
			first = false;
		}	

        if ( AXZ_SUCCESS( rc ) ) {	
		    this->m_json += L"\n";
		    for ( int i = 0; i < this->m_indent ; i++)
		    {
			    this->m_json += L" ";
		    }
		    this->m_json += L"}";		
		}
		return rc;
	}
    //
    //-------------< End - AxzNiceJsonStepper Implementation >----------------------

	//-------------< Start - AxzJsonBuilder Implementation >----------------------
	//
	namespace AxzJsonBuilder
    {
	    axz_rc build( axz_wstring json_in, AxzDict & json_value )
	    {
		    // validate against null
		    if( json_in.empty() )
		    {
			    //return WAAPI_LOG_RET( WAAPI_ERROR_INVALID_JSON, L"Cannot build JSON from an empty string" );
			    return AXZ_ERROR_INVALID_INPUT;
		    }

		    // clear all new lines so we are working with 1 constant string
		    AxzJsonBuilder::_replaceChars( json_in, L"\n\r\t", L' ');

		    // pass to recursive creator
		    std::size_t pos = 0;
		    axz_int rc = AxzJsonBuilder::_build( json_in, pos, json_value );
		    if( AXZ_FAILED( rc ) )
		    {
			    // if failed, clear values
			    json_value.clear();
			    //return WAAPI_LOG_RET( rc );
			    return rc;
		    } 
		    else
		    {
			    // clear any left behind trailing whitespace
			    AxzJsonBuilder::_ignoreWhiteSpace( json_in, pos );
		    }
		    // check for any additional trailing content
		    if( pos < json_in.length() )
		    {
			    json_value.clear();
			    //return WAAPI_LOG_RET( WAAPI_ERROR_INVALID_JSON, L"JSON trailing content cannot guarantee accuracy of intended JSON" );
			    return AXZ_ERROR_INVALID_INPUT;
		    }

		    return AXZ_OK;
	    }

	    axz_rc _build( const axz_wstring & str, size_t & pos, AxzDict & json_value )
	    {
		    // ignore all white space and non-ascii we dislike
		    AxzJsonBuilder::_ignoreWhiteSpace( str, pos );

		    // handle special wrapping case
		    if( str[ pos ] == L'(' )
		    {
			    pos++;
		    }

		    // holder for converting strings
		    axz_wstring json_string;

		    switch( str[ pos ] )
		    {
		    case L'{' :
			    //return WAAPI_LOG_FUNC_RET( _buildObject( str, ++pos, json_value ) );
			    return AxzJsonBuilder::_buildObject( str, ++pos, json_value );
		    case L'[':
			    return AxzJsonBuilder::_buildArray( str, ++pos, json_value );
		    case L'"':
			    if( AXZ_FAILED( AxzJsonBuilder::_buildString( str, ++pos, json_string ) ) )
			    {
				    return AXZ_ERROR_INVALID_INPUT;
			    }
			    json_value = json_string.c_str();
			    break;
		    // handle the special cases for true, false and null
		    case L't':
		    case L'f':
		    case L'n':
			    return AxzJsonBuilder::_buildSpecialCase( str, ++pos, json_value );
		    default:
			    return AxzJsonBuilder::_buildNumber( str, pos, json_value );
		    }
		    return AXZ_OK;

	    }

	    void _ignoreWhiteSpace( const axz_wstring & str, size_t & pos )
	    {
		    // we do not want any white space or special characters (e.g. new line feeds, carriage returns, etc. )
		    // take a look at an ASCII chart http://www.asciitable.com/
		    while( pos < str.length() && /*pos != axz_wstring::npos &&*/ str[ pos ] <= 0x20 )
		    {
			    pos++;
		    }
	    }

	    axz_rc _buildObject( const axz_wstring & str, size_t & pos, AxzDict & json_object )
	    {
		    // make the passed in value an empty object
		    json_object.become( AxzDictType::OBJECT );

		    // while we have not hit the end of our string continue to grab object data
		    while( pos != axz_wstring::npos )
		    {
			    axz_wstring key;
			    AxzDict json_value;

			    // skip white space
			    AxzJsonBuilder::_ignoreWhiteSpace( str, pos );

			    // found the end of our map so we can return now
			    if( str[ pos ] == L'}' && pos++ )
			    {
				    return AXZ_OK;
			    }

			    if( str[ pos ] == L'"' && pos++ )
			    {
				    // parse out the key for this map entry
				    if( AXZ_FAILED( AxzJsonBuilder::_buildString( str, pos, key ) ) )
				    {
					    return AXZ_ERROR_INVALID_INPUT;
				    }
			    }
			    else
			    {
				    return AXZ_ERROR_INVALID_INPUT;
			    }

			    // skip white space
			    AxzJsonBuilder::_ignoreWhiteSpace( str, pos );

			    // we must have the value now for our map which is separated by ':'
			    // if this is not the next character in the string then we have failed
			    if( str[ pos ] != L':' )
			    {
				    return AXZ_ERROR_INVALID_INPUT;
			    }

			    // move past the ':'
			    ++pos;

			    // retrieve the value for the map key
			    if( AXZ_FAILED( AxzJsonBuilder::_build( str, pos, json_value ) ) )
			    {
				    return AXZ_ERROR_INVALID_INPUT;
			    }

			    // update our JSON instance with the mapped value
			    json_object.add( key, std::move( json_value ) );

			    // skip white space
			    AxzJsonBuilder::_ignoreWhiteSpace( str, pos );

			    // check if we have a comma to check for, meaning we have multiple keys
			    if( str[ pos ] == L',' && pos++ )
			    {
				    // check if our comma is a trailing comma, meaning it does not split an object which defines improper JSON format
				    // if that is the case then we have failed
				    AxzJsonBuilder::_ignoreWhiteSpace( str, pos );
				    if( str[ pos ] == L'}' )
				    {
					    return AXZ_ERROR_INVALID_INPUT;
				    }
			    }
		    }

		    return AXZ_OK;
	    }

	    axz_rc _buildString( const axz_wstring & str, size_t & pos, axz_wstring & json_string )
	    {
		    json_string = L"";
		    do 
		    {
			    // we found the end of the string
			    if( str[ pos ] == L'"' )
			    {
				    // increment and success
				    pos++;
				    return AXZ_OK;
			    }
			    else if( str[ pos ] == L'\\' ) // escaped characters
			    {
				    axz_wchar next = str[ ++pos ];
				    switch( next )
				    {
				    case L'\\':
				    case L'/':
				    case L'"':
					    break;
				    case L'b':   
					    next = L'\b';  
					    break;
				    case L'n':   
					    next = L'\n';  
					    break;
				    case L'r':   
					    next = L'\r';  
					    break;
				    case L't':   
					    next = L'\t';  
					    break;
				    case L'f':   
					    next = L'\f';  
					    break; 
				    case L'u':
					    // must escape in place
					    axz_int rc;
					    if( AXZ_FAILED( rc = AxzJsonBuilder::_decodeCharacter( str, next, ++pos ) ) )
					    {
						    return rc;
					    }
					    break;
				    }
				    json_string += next;
				    pos++;
			    }
			    else if( str[pos ] < 0x20 ) // un-escaped character! means invalid JSON string
			    {
				    return AXZ_ERROR_INVALID_INPUT;
			    }
			    else
			    {
				    json_string += str[ pos++ ];
			    }
		    } 
		    while ( pos != axz_wstring::npos );

		    // failed to find the end of the string, therefore invalid JSON
		    return AXZ_ERROR_INVALID_INPUT;
	    }

	    axz_rc _decodeCharacter( const axz_wstring & json_string, axz_wchar & json_decoded_character, size_t & pos )
	    {
		    if( pos + 4 > json_string.length() )
		    {
			    return AXZ_ERROR_INVALID_INPUT;
		    }

		    if( AXZ_FAILED( AxzJsonBuilder::_convertToHexQuad( json_string.substr( pos, 4 ), json_decoded_character ) ) )
		    {
			    return AXZ_ERROR_INVALID_INPUT;
		    }

		    pos += 4;

		    // wchar_t is 2 bytes on windows, while its 4 bytes on mac and linux. 
		    // windows cannot represent a surrogate pair like mac and linux, thus windows leaves these as pairs	

		    // high surrogate char?
		    if( json_decoded_character >= 0xd800 )
		    {
			    // yes - expect a low char
			    if( json_decoded_character < 0xdc00 )
			    {
				    axz_wchar low_character;
				    if( !( json_string[ pos ] == L'\\' && ++pos && json_string[ pos ] == L'u' && ++pos && AXZ_SUCCESS( AxzJsonBuilder::_convertToHexQuad( json_string.substr( pos, 4 ), low_character ) ) ) )
				    {
					    // missing low character in surrogate pair
					    return AXZ_ERROR_INVALID_INPUT;
				    }

				    pos += 4;

				    if( low_character < 0xdc00 || low_character >= 0xdfff )
				    {
					    // invalid low surrogate pair
					    return AXZ_ERROR_INVALID_INPUT;
				    }

				    json_decoded_character = ( json_decoded_character - 0xd800 ) * 0x400 + ( low_character - 0xdc00 ) + 0x10000;
			    }
			    else if( json_decoded_character < 0xe000 )
			    {
				    // invalid high character in surrogate pair
				    return AXZ_ERROR_INVALID_INPUT;
			    }
		    }	

		    // hacky fix to check string iteration in check, see buildString()
		    pos--;
		    return  AXZ_OK;
	    }


	    axz_rc _convertToHexQuad( const axz_wstring & str, axz_wchar & converted )
	    {
		    // check if we have the proper input
		    if( str.size() != 4 )
		    {
			    return AXZ_ERROR_INVALID_INPUT;
		    }

		    // initialize
		    converted = 0;

		    // iterate over each HEX value
		    for( size_t pos = 0; pos < 4; ++pos )
		    {
			    axz_wchar c = str[ pos ];

			    // Check algorithm for converting HEX to char taken fro SBJson (Objective-C) parser
			    int check = ( c >= '0' && c <= '9' )
				    ? c - '0' : ( c >= 'a' && c <= 'f' )
				    ? ( c - 'a' + 10 ) : (c >= 'A' && c <= 'F' )
				    ? ( c - 'A' + 10 ) : -1;
			    if( check == -1 )
			    {
				    return AXZ_ERROR_INVALID_INPUT;
			    }
			    converted *= 16;
			    converted += check;
		    }

		    // converted just fine
		    return AXZ_OK;
	    }

	    axz_rc _buildArray( const axz_wstring & str, size_t & pos, AxzDict& json_array )
	    {
		    // convert to empty array
		    json_array.become( AxzDictType::ARRAY );		

		    while( pos != axz_wstring::npos )
		    {
			    AxzJsonBuilder::_ignoreWhiteSpace( str, pos );

			    // check if we hit the end of our array
			    if( str[ pos ] == L']' && pos++ )
			    {
				    return AXZ_OK;
			    }

			    // convert JSON value held by array
			    AxzDict json_value;
			    if( AXZ_FAILED( AxzJsonBuilder::_build( str, pos, json_value ) ) )
			    {
				    return AXZ_ERROR_INVALID_INPUT;
			    }

			    // update array
			    json_array.add( json_value );

			    AxzJsonBuilder::_ignoreWhiteSpace( str, pos );

			    // check for additional items
			    if( str[ pos ] == L',' && pos++ )
			    {
				    AxzJsonBuilder::_ignoreWhiteSpace( str, pos );
				    if( str[ pos ] == L',' )
				    {
					    // nope! invalid trailing comma. Invalid JSON format
					    return AXZ_ERROR_INVALID_INPUT;
				    }
				    if( str[pos] == L']' )
				    {
					    return AXZ_ERROR_INVALID_INPUT;
				    }
			    }
		    }
		    // the only way we get here is if we hit the end of the file which cannot happen without ending the array in valid JSON format
		    return AXZ_ERROR_INVALID_INPUT;
	    }

	    axz_rc _buildSpecialCase( const axz_wstring & str, size_t & pos, AxzDict & json_special )
	    {
		    // check bounds
		    if( pos + 3 > str.length() )
		    {
			    return AXZ_ERROR_INVALID_INPUT;
		    }

		    // substring out the space check
		    axz_wstring tmp = str.substr( pos, 3 );

		    // true case
		    if( tmp == L"rue" )
		    {
			    json_special = true;
			    pos += 3;
			    return AXZ_OK;
		    }
		    else if( tmp == L"ull" )
		    {
			    json_special.clear(); // make null
			    pos += 3;
			    return AXZ_OK;
		    }
		    else
		    {
			    // check bounds
			    if( pos + 4 > str.length() )
			    {
				    return AXZ_ERROR_INVALID_INPUT;
			    }
			    tmp = str.substr( pos , 4 );
			    if( tmp == L"alse" )
			    {
				    json_special = false;
				    pos += 4;
				    return AXZ_OK;
			    }
		    }
		    // invalid special case
		    return AXZ_ERROR_INVALID_INPUT;
	    }

	    axz_rc _buildNumber( const axz_wstring & str, size_t & pos, AxzDict & json_number )
	    {
		    size_t firstPos = pos;
		    bool bIsComplex = false;
		    axz_wstring tmp = L"";

		    while( iswdigit( str[ pos ] ) 
			    || str[ pos ] == L'-' 
			    || str[ pos ] == L'+' 
			    || str[ pos ] == L'.'
			    || str[ pos ] == L'e'
			    || str[ pos ] == L'E' )
		    {
			    // complex numbers need to be tracked and will be returned as string values
			    if( str[ pos ] == L'.'
				    || str[ pos ] == L'e'
				    || str[ pos ] == L'E'  )
			    {
				    bIsComplex = true;
			    }
		
			    tmp += str[ pos++ ];
		    }

		    size_t size = tmp.size();
		    // an empty number or only one of -,+,.,e,E is an error
		    if( tmp.empty() || (size == 1 && !iswdigit( tmp[0] )) )
		    {
			    return AXZ_ERROR_INVALID_INPUT;
		    }

		    bool isNegative = (tmp[0] == L'-'),
			     isValidInt = false; // assume its not

		    // convert to number if valid, otherwise keep as string value
		    if( !bIsComplex	&& ( size < 10 || (size == 10 && isNegative) ) )
		    {
			    isValidInt = true;
		    }
		    // check for larger range ints separately, since they are more complex to verify
		    else if( !bIsComplex && ( size == 10 || (size == 11 && isNegative) ) )
		    {
			    // need to ensure integer value is valid range, i.e. INT_MAX = 2147483647, INT_MIN = -2147483648

			    // get first 9 digits and compare against max/min minus last digit
			    const int first9 = 214748364;
			    axz_wstring partial;
			    if( isNegative )
				    partial = tmp.substr( 1, 9 );
			    else
				    partial = tmp.substr( 0, 9 );

			    axz_int partialInt = std::stoi( partial );
			    if( partialInt < first9 )
			    {
				    if ( ( firstPos < pos && iswdigit( str[pos - 1] )) || ( str[ pos] == L'-' && pos == firstPos ) )
				    isValidInt = true;
			    }
			    else if( partialInt == first9 ) // need to check last digit
			    {
				    // already validated that the length is 10 or 11
				    if( isNegative )
				    {
					    int last = tmp[ 10 ] - L'0';
					    if( last <= 8 ) // last of INT_MIN
						    isValidInt = true;
				    }
				    else
				    {
					    int last = tmp[ 9 ] - L'0';
					    if( last <= 7 ) // last of INT_MAX
						    isValidInt = true;
				    }
			    }
		    }

		    if( isValidInt )
			    json_number = ( int32_t )std::stoll( tmp );
		    else
			    json_number = tmp.c_str();

		    return AXZ_OK;
	    }

	    void _replaceChars( axz_wstring& inout_src, const axz_wstring& in_delimiters, axz_wchar in_replaced )
	    {
		    for ( auto delimiter: in_delimiters )
		    {
			    std::replace( inout_src.begin(), inout_src.end(), delimiter, in_replaced );
		    }
	    }
    }
   	//-------------< End - AxzJsonBuilder Implementation >----------------------
}
}
