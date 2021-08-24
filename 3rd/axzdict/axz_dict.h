#ifndef __axz_dict__
#define __axz_dict__

#include "axz_types.h"
#include "axz_export.h"
#include <set>
#include <unordered_map>
#include <memory>
#include <functional>

#ifdef _MSC_VER
#	pragma warning( push )
#	pragma warning( disable: 4251 )
#endif

enum class AxzDictType 
{
	NUL, 
	NUMBER, 
	BOOL, 
	STRING,
	BYTES,
	ARRAY, 
	OBJECT,
	CALLABLE
};

class AxzDict;
using axz_dict_array    = std::vector<AxzDict>;
using axz_dict_object   = std::unordered_map<axz_wstring, AxzDict>;
using axz_dict_keys     = std::set<axz_wstring>;
using axz_dict_callable = std::function<AxzDict ( AxzDict&& )>;

using axz_shared_dict   = std::shared_ptr<AxzDict>;
using axz_weak_dict     = std::weak_ptr<AxzDict>;
using axz_unique_dict   = std::unique_ptr<AxzDict>;

class _AxzDicVal;
class AxzDictStepper;

class AXZDICT_DECLCLASS AxzDict final
{
public:
        AxzDict() noexcept;
        AxzDict( AxzDictType type ) noexcept;
        AxzDict( void* ) = delete;
        AxzDict( std::nullptr_t ) noexcept;
        AxzDict( double val );
        AxzDict( int32_t val );
        AxzDict( bool val );
        AxzDict( const axz_wstring& val );
        AxzDict( const axz_wchar* val );
        AxzDict( axz_wstring&& val );

        AxzDict( const axz_bytes& val );
        AxzDict( axz_bytes&& val );

        AxzDict( const axz_dict_array& vals );
        AxzDict( axz_dict_array&& vals );

        AxzDict( const axz_dict_object& vals );
        AxzDict( axz_dict_object&& vals );

        AxzDict( const AxzDict& val );
        AxzDict( AxzDict&& val );

        AxzDict( const axz_dict_callable& val );

        ~AxzDict() = default;

        AxzDict& operator=( const AxzDict& other );
        AxzDict& operator=( AxzDict&& other );

        AxzDict& operator=( std::nullptr_t );
        AxzDict& operator=( void* ) = delete;
        AxzDict& operator=( double val );
        AxzDict& operator=( int32_t val );
        AxzDict& operator=( bool val );
        AxzDict& operator=( const axz_wstring& val );
        AxzDict& operator=( axz_wstring&& val );
        AxzDict& operator=( const axz_wchar* val );
        AxzDict& operator=( const axz_bytes& val );
        AxzDict& operator=( axz_bytes&& val );

        AxzDict& operator=( const axz_dict_object& other );
        AxzDict& operator=( axz_dict_object&& other );

        AxzDict& operator=( const axz_dict_array& other );
        AxzDict& operator=( axz_dict_array&& other );

        AxzDict& operator=( const axz_dict_callable& other );

        AxzDictType type() const;
        bool isType( const AxzDictType type ) const;

        bool isNull()		const;
        bool isNumber() 	const;
        bool isString()		const;
        bool isBytes()		const;
        bool isArray()		const;
        bool isObject() 	const;
        bool isCallable() 	const;

        axz_rc val( double& val ) const;
        axz_rc val( int32_t& val ) const;
        axz_rc val( bool& val ) const;
        axz_rc val( axz_wstring& val ) const;
        axz_rc val( axz_bytes& val ) const;

        axz_rc steal( double& val );
        axz_rc steal( int32_t& val );
        axz_rc steal( bool& val );
        axz_rc steal( axz_wstring& val );
        axz_rc steal( axz_bytes& val );

        axz_rc val( const axz_wstring& key, AxzDict& val ) const; 		// copy to val
        axz_rc val( const axz_wstring& key, double& val ) const;
        axz_rc val( const axz_wstring& key, int32_t& val ) const;
        axz_rc val( const axz_wstring& key, bool& val ) const;
        axz_rc val( const axz_wstring& key, axz_wstring& val ) const;
        axz_rc val( const axz_wstring& key, axz_bytes& val ) const;

        /* apply dot syntax to copy data out*/
        axz_rc dotVal( const axz_wstring& key_list, AxzDict& val ) const;
        axz_rc dotVal( const axz_wstring& key_list, double& val ) const;
        axz_rc dotVal( const axz_wstring& key_list, int32_t& val ) const;
        axz_rc dotVal( const axz_wstring& key_list, bool& val ) const;
        axz_rc dotVal( const axz_wstring& key_list, axz_wstring& val ) const;
        axz_rc dotVal( const axz_wstring& key_list, axz_bytes& val ) const;

        /* steal data from value of key - not applicable for const object*/
        axz_rc steal( const axz_wstring& key, AxzDict& val );
        axz_rc steal( const axz_wstring& key, double& val );
        axz_rc steal( const axz_wstring& key, int32_t& val );
        axz_rc steal( const axz_wstring& key, bool& val );
        axz_rc steal( const axz_wstring& key, axz_wstring& val );
        axz_rc steal( const axz_wstring& key, axz_bytes& val );

        /* apply dot syntax to steal data out - not applicable for const object*/
        axz_rc dotSteal( const axz_wstring& key_list, AxzDict& val );
        axz_rc dotSteal( const axz_wstring& key_list, double& val );
        axz_rc dotSteal( const axz_wstring& key_list, int32_t& val );
        axz_rc dotSteal( const axz_wstring& key_list, bool& val );
        axz_rc dotSteal( const axz_wstring& key_list, axz_wstring& val );
        axz_rc dotSteal( const axz_wstring& key_list, axz_bytes& val );

        axz_rc val( const size_t idx, AxzDict& val ) const;				// copy to val
        axz_rc val( const size_t idx, double& val ) const;
        axz_rc val( const size_t idx, int32_t& val ) const;
        axz_rc val( const size_t idx, bool& val ) const;
        axz_rc val( const size_t idx, axz_wstring& val ) const;
        axz_rc val( const size_t idx, axz_bytes& val ) const;

        /* steal data from value of key*/
        axz_rc steal( const size_t idx, AxzDict& val );				// unapplicable for const object
        axz_rc steal( const size_t idx, double& val );
        axz_rc steal( const size_t idx, int32_t& val );
        axz_rc steal( const size_t idx, bool& val );
        axz_rc steal( const size_t idx, axz_wstring& val );			// unapplicable for const object
        axz_rc steal( const size_t idx, axz_bytes& val );			// unapplicable for const object

        /* appicale for callable object*/
        axz_rc call( AxzDict&& in_val, AxzDict& out_val ) const;
        AxzDict operator()( AxzDict&& val ) const;

        double		numberVal() const;
        int32_t		intVal()	const;
        bool		boolVal()	const;
        axz_wstring	stringVal() const;
        axz_bytes	bytesVal()	const;

        size_t size() const;
        const AxzDict& operator[]( const size_t idx ) const;
        AxzDict& operator[]( const size_t idx );

        const AxzDict& operator[]( const axz_wstring& key ) const;
        AxzDict& operator[]( const axz_wstring& key );

        bool operator==(const AxzDict& other) const;
        bool operator!=(const AxzDict& other) const;

        axz_rc add( const AxzDict& val );
        axz_rc add( AxzDict&& val );
        axz_rc add( const axz_wstring& key, const AxzDict& val );
        axz_rc add( const axz_wstring& key, AxzDict&& val );

        void clear();	// remove the internal data only, not reset to null
        axz_rc remove( const size_t idx );
        axz_rc remove( const axz_wstring& key );

        axz_rc contain( const axz_wstring& key ) const;
        axz_rc contain( const axz_wstring& key, const AxzDictType type ) const;

        axz_dict_keys keys() const;
        void become( AxzDictType type );		// drop internal data and turn to new empty type
        void drop();							// drop internal data and turn to null
    axz_rc step( std::shared_ptr<AxzDictStepper> stepper ) const;

private:
        AxzDict( std::shared_ptr<_AxzDicVal> other ): m_val( other ){}
        void _set( const AxzDict& val );
        void _set( AxzDict&& val );

private:
        std::shared_ptr<_AxzDicVal> m_val;
        friend class _AxzDot;
};

#ifdef _MSC_VER
#	pragma warning( pop )
#endif

#endif
