#include "axz_dict.h"
#include "axz_dict_stepper.h"
#include "axz_error_codes.h"
#include <cassert>
#include <cstring>
#include <stdexcept>

template<bool isDict>
struct _AxzBool2Type
{
	enum{ val = isDict };
};

class _AxzDictVal;
struct _AxzDicDefault 
{	
	static const std::shared_ptr<_AxzDicVal> nullVal;
	static const std::shared_ptr<_AxzDicVal> trueVal;
	static const std::shared_ptr<_AxzDicVal> falseVal;
};

namespace
{
namespace Internal
{
	template<class T>
	axz_rc getVal( AxzDict& in_object, T& out_val, _AxzBool2Type<false> )
	{
		return in_object.val( out_val );
	}
	
	axz_rc getVal( AxzDict& in_object, AxzDict& out_val, _AxzBool2Type<true> )
	{
		out_val = in_object;
		return AXZ_OK;
	}	

	template<class T>
	axz_rc stealVal( AxzDict& in_object, T& out_val, _AxzBool2Type<false> )
	{
		axz_rc rc = in_object.steal( out_val );
		if ( AXZ_SUCCESS( rc ) )	in_object.drop();
		return rc;
	}

	axz_rc stealVal( AxzDict& in_object, AxzDict& out_val, _AxzBool2Type<true> )
	{
		out_val = std::move( in_object );
		in_object.drop();
		return AXZ_OK;
	}
}
};

class _AxzDicVal
{
public:
        virtual AxzDictType type() const = 0;
        virtual bool isType( const AxzDictType type ) const = 0;
        virtual bool equals(const _AxzDicVal& other) const = 0;
        virtual size_t size() const 					{ throw std::invalid_argument( "AxzDict::size available for object or array only" ); }
        virtual axz_rc val( double& val ) const                     	    	{ return AXZ_ERROR_NOT_SUPPORT; }
        virtual axz_rc val( int32_t& val ) const		                        { return AXZ_ERROR_NOT_SUPPORT; };
        virtual axz_rc val( bool& val ) const		                            { return AXZ_ERROR_NOT_SUPPORT; };
        virtual axz_rc val( axz_wstring& val ) const	                        { return AXZ_ERROR_NOT_SUPPORT; };
        virtual axz_rc val( axz_bytes& val ) const	                            { return AXZ_ERROR_NOT_SUPPORT; };

        virtual axz_rc steal( double& val )    		                 	    	{ return this->val( val ); }
        virtual axz_rc steal( int32_t& val )			                        { return this->val( val ); };
        virtual axz_rc steal( bool& val ) 			                            { return this->val( val ); };
        virtual axz_rc steal( axz_wstring& val ) 		                        { return AXZ_ERROR_NOT_SUPPORT; };
        virtual axz_rc steal( axz_bytes& val ) 		                        	{ return AXZ_ERROR_NOT_SUPPORT; };

        virtual axz_rc val( const axz_wstring& key, AxzDict& val )	            { return AXZ_ERROR_NOT_SUPPORT; };
        virtual axz_rc val( const axz_wstring& key, double& val )	    		{ return AXZ_ERROR_NOT_SUPPORT; };
        virtual axz_rc val( const axz_wstring& key, int32_t& val )		   	    { return AXZ_ERROR_NOT_SUPPORT; };
        virtual axz_rc val( const axz_wstring& key, bool& val )		    	    { return AXZ_ERROR_NOT_SUPPORT; };
        virtual axz_rc val( const axz_wstring& key, axz_wstring& val )	    	{ return AXZ_ERROR_NOT_SUPPORT; };
        virtual axz_rc val( const axz_wstring& key, axz_bytes& val )		    { return AXZ_ERROR_NOT_SUPPORT; };

        virtual axz_rc steal( const axz_wstring& key, AxzDict& val )			{ return AXZ_ERROR_NOT_SUPPORT; };
        virtual axz_rc steal( const axz_wstring& key, double& val )	    		{ return AXZ_ERROR_NOT_SUPPORT; };
        virtual axz_rc steal( const axz_wstring& key, int32_t& val )		   	{ return AXZ_ERROR_NOT_SUPPORT; };
        virtual axz_rc steal( const axz_wstring& key, bool& val )		    	{ return AXZ_ERROR_NOT_SUPPORT; };
        virtual axz_rc steal( const axz_wstring& key, axz_wstring& val )		{ return AXZ_ERROR_NOT_SUPPORT; };
        virtual axz_rc steal( const axz_wstring& key, axz_bytes& val )			{ return AXZ_ERROR_NOT_SUPPORT; };

        virtual axz_rc val( const size_t idx, AxzDict& val )            	    { return AXZ_ERROR_NOT_SUPPORT; };
        virtual axz_rc val( const size_t idx, double& val )			            { return AXZ_ERROR_NOT_SUPPORT; };
        virtual axz_rc val( const size_t idx, int32_t& val )				    { return AXZ_ERROR_NOT_SUPPORT; };
        virtual axz_rc val( const size_t idx, bool& val )			            { return AXZ_ERROR_NOT_SUPPORT; };
        virtual axz_rc val( const size_t idx, axz_wstring& val )		        { return AXZ_ERROR_NOT_SUPPORT; };
        virtual axz_rc val( const size_t idx, axz_bytes& val )		            { return AXZ_ERROR_NOT_SUPPORT; };

        virtual axz_rc steal( const size_t idx, AxzDict& val )					{ return AXZ_ERROR_NOT_SUPPORT; };
        virtual axz_rc steal( const size_t idx, double& val )			        { return AXZ_ERROR_NOT_SUPPORT; };
        virtual axz_rc steal( const size_t idx, int32_t& val )				    { return AXZ_ERROR_NOT_SUPPORT; };
        virtual axz_rc steal( const size_t idx, bool& val )			            { return AXZ_ERROR_NOT_SUPPORT; };
        virtual axz_rc steal( const size_t idx, axz_wstring& val )				{ return AXZ_ERROR_NOT_SUPPORT; };
        virtual axz_rc steal( const size_t idx, axz_bytes& val )				{ return AXZ_ERROR_NOT_SUPPORT; };

        virtual axz_rc call( AxzDict&& in_val, AxzDict& out_val )				{ return AXZ_ERROR_NOT_SUPPORT; };

        virtual double numberVal() const            							{ throw std::invalid_argument( "AxzDict::numberVal available for number only" ); }
        virtual int32_t intVal() const                  						{ throw std::invalid_argument( "AxzDict::intVal available for number only" ); }
        virtual bool boolVal() const                							{ throw std::invalid_argument( "AxzDict::boolVal available for boolean only" ); }
        virtual axz_wstring stringVal() const									{ throw std::invalid_argument( "AxzDict::stringVal available for string only" ); }
        virtual axz_bytes bytesVal() const										{ throw std::invalid_argument( "AxzDict::bytesVal available for bytes only" ); }
        virtual const axz_dict_array& arrayVal() const										{ throw std::invalid_argument( "AxzDict::arrayVal available for array only" ); }
        virtual const axz_dict_object& objectVal() const										{ throw std::invalid_argument( "AxzDict::objectVal available for object only" ); }

        virtual axz_rc add( const AxzDict& val )	                            { return AXZ_ERROR_NOT_SUPPORT; };
        virtual axz_rc add( AxzDict&& val )			                            { return AXZ_ERROR_NOT_SUPPORT; };
        virtual axz_rc add( const axz_wstring& key, const AxzDict& val )        { return AXZ_ERROR_NOT_SUPPORT; };
        virtual axz_rc add( const axz_wstring& key, AxzDict&& val )		        { return AXZ_ERROR_NOT_SUPPORT; };

        virtual void clear() {};
        virtual axz_rc remove( const size_t idx )		                        { return AXZ_ERROR_NOT_SUPPORT; };
        virtual axz_rc remove( const axz_wstring& key )	                        { return AXZ_ERROR_NOT_SUPPORT; };

        virtual axz_rc contain( const axz_wstring& key )	                    { return AXZ_ERROR_NOT_SUPPORT; };
        virtual axz_rc contain( const axz_wstring& key, const AxzDictType type ){ return AXZ_ERROR_NOT_SUPPORT; };

        virtual AxzDict& at( const size_t idx )									{ throw std::invalid_argument( "AxzDict::at@size_t available for array only" ); }
        virtual AxzDict& at( const axz_wstring& key )							{ throw std::invalid_argument( "AxzDict::at@axz_wstring available for object only" ); }

        virtual axz_rc step( axz_shared_dict_stepper stepper ) = 0;
};

template<AxzDictType _type, class T>
class _AxzTVal: public _AxzDicVal
{
public:
        explicit _AxzTVal( const T& val ): m_val( val ) {}
        explicit _AxzTVal( T&& val ): m_val( std::move( val ) ) {}
        ~_AxzTVal() {}

        T& data() { return this->m_val; }
        AxzDictType type() const override								        { return _type; }
        bool equals(const _AxzDicVal& other) const override {
          return m_val == static_cast<const _AxzTVal<_type, T>*>(&other)->m_val;
        }

        virtual bool isType( const AxzDictType type ) const override	        { return ( _type == type ); }
        virtual axz_rc step( axz_shared_dict_stepper stepper ) override {
                return stepper->step( this->data() );
        }

protected:
        T m_val;
};

class _AxzNull final: public _AxzTVal< AxzDictType::NUL, std::nullptr_t >
{
public:
        _AxzNull(): _AxzTVal( nullptr ) {}
};

class _AxzBool final: public _AxzTVal< AxzDictType::BOOL, bool >
{
public:
        explicit _AxzBool( bool val = true ): _AxzTVal( val ) {}
        virtual axz_rc val( bool& val ) const override	                        { val = this->m_val; return AXZ_OK; }
        virtual bool boolVal() const override			                        { return this->m_val; }
};

const std::shared_ptr<_AxzDicVal> _AxzDicDefault::nullVal = std::make_shared<_AxzNull>();
const std::shared_ptr<_AxzDicVal> _AxzDicDefault::trueVal = std::make_shared<_AxzBool>( true );
const std::shared_ptr<_AxzDicVal> _AxzDicDefault::falseVal = std::make_shared<_AxzBool>( false );

class _AxzDouble final: public _AxzTVal< AxzDictType::NUMBER, double >
{
public:
        explicit _AxzDouble( double val = 0. ): _AxzTVal( val ) {}

        virtual axz_rc val( double& val ) const override
        {
                val = this->m_val;
                return AXZ_OK;
        }

        virtual axz_rc val( int32_t& val ) const override
        {
                val = ( int32_t )this->m_val;
                return AXZ_OK;
        }
        bool equals(const _AxzDicVal& other) const override {
          return ::abs(m_val - other.numberVal()) < std::numeric_limits<double>::epsilon();
        }
        virtual double numberVal() const override	{ return this->m_val; }
        virtual int32_t intVal() const override		{ return ( int32_t )this->m_val; }
};

class _AxzInt final: public _AxzTVal< AxzDictType::NUMBER, int32_t >
{
public:
        explicit _AxzInt( int32_t val = 0 ): _AxzTVal( val ) {}

        virtual axz_rc val( double& val ) const override
        {
                val = ( double )this->m_val;
                return AXZ_OK;
        }

        virtual axz_rc val( int32_t& val ) const override
        {
                val = this->m_val;
                return AXZ_OK;
        }
        bool equals(const _AxzDicVal& other) const override {
            return ::abs(m_val - other.numberVal()) < std::numeric_limits<double>::epsilon();
        }
        virtual double numberVal() const override	{ return ( double )this->m_val; }
        virtual int32_t intVal() const override		{ return this->m_val; }
};

class _AxzString final: public _AxzTVal< AxzDictType::STRING, axz_wstring >
{
public:
        explicit _AxzString( const axz_wstring& val = L"" ): _AxzTVal( val )    {}
        explicit _AxzString( axz_wstring&& val ): _AxzTVal( std::move( val ) )  {}

        virtual axz_rc val( axz_wstring& val ) const override	                { val = this->m_val; return AXZ_OK; }
        virtual axz_rc steal( axz_wstring& val ) override						{ val = std::move( this->m_val ); return AXZ_OK; }
        virtual axz_wstring stringVal() const override		                    { return this->m_val; }
        virtual void clear() override						                    { this->m_val.clear(); }
};

class _AxzBytes final: public _AxzTVal< AxzDictType::BYTES, axz_bytes >
{
public:
        explicit _AxzBytes( const axz_bytes& val = {} ): _AxzTVal( val )        {}
        explicit _AxzBytes( axz_bytes&& val ): _AxzTVal( std::move( val ) )     {}

        virtual axz_rc val( axz_bytes& val ) const override	                    { val = this->m_val; return AXZ_OK; }
        virtual axz_rc steal( axz_bytes& val ) override							{ val = std::move( this->m_val ); return AXZ_OK; }

        virtual axz_bytes bytesVal() const override			                    { return this->m_val; }
        virtual void clear() override						                    { this->m_val.clear(); }
};

class _AxzArray final: public _AxzTVal< AxzDictType::ARRAY, axz_dict_array >
{
public:
        explicit _AxzArray( const axz_dict_array& val = {} ): _AxzTVal( val ) {}
        explicit _AxzArray( axz_dict_array&& val ): _AxzTVal( std::move( val ) ) {}

        virtual axz_rc val( const size_t idx, AxzDict& val ) override			{ return this->_val<AxzDict, true>( idx, val ); }
        virtual axz_rc val( const size_t idx, double& val ) override			{ return this->_val<double, false>( idx, val ); }
        virtual axz_rc val( const size_t idx, int32_t& val ) override   		{ return this->_val<int32_t, false>( idx, val ); }
        virtual axz_rc val( const size_t idx, bool& val ) override                      { return this->_val<bool, false>( idx, val ); }
        virtual axz_rc val( const size_t idx, axz_wstring& val ) override		{ return this->_val<axz_wstring, false>( idx, val ); }
        virtual axz_rc val( const size_t idx, axz_bytes& val ) override                 { return this->_val<axz_bytes, false>( idx, val ); }
        virtual const axz_dict_array& arrayVal() const override                                { return this->m_val; }
        virtual axz_rc steal( const size_t idx, AxzDict& val ) override			{ return this->_steal<AxzDict, true>( idx, val ); }
        virtual axz_rc steal( const size_t idx, double& val ) override			{ return this->_steal<double, false>( idx, val ); }
        virtual axz_rc steal( const size_t idx, int32_t& val ) override   		{ return this->_steal<int32_t, false>( idx, val ); }
        virtual axz_rc steal( const size_t idx, bool& val ) override			{ return this->_steal<bool, false>( idx, val ); }
        virtual axz_rc steal( const size_t idx, axz_wstring& val ) override		{ return this->_steal<axz_wstring, false>( idx, val ); }
        virtual axz_rc steal( const size_t idx, axz_bytes& val ) override		{ return this->_steal<axz_bytes, false>( idx, val ); }

        virtual void clear() override										    { this->m_val.clear(); }
        virtual size_t size() const override								    { return this->m_val.size(); }

        virtual axz_rc add( const AxzDict& val ) override
        {
                this->m_val.emplace_back( val );
                return AXZ_OK;
        }

        virtual axz_rc add( AxzDict&& val )	override
        {
                this->m_val.emplace_back( std::move( val ) );
                return AXZ_OK;
        }

        virtual axz_rc remove( const size_t idx ) override
        {
                if ( idx < 0 || idx >= this->m_val.size() )
                        return AXZ_ERROR_OUT_OF_RANGE;

                this->m_val.erase( this->m_val.begin() + idx );
                return AXZ_OK;
        }

        const AxzDict& at( const size_t idx ) const
        {
                assert( ( 0 <= idx && idx < this->m_val.size() ) );
                return this->m_val[idx];
        }

        AxzDict& at( const size_t idx )
        {
                assert( ( 0 <= idx && idx < this->m_val.size() ) );
                return this->m_val[idx];
        }

private:

        template<class V, bool isDict>
        axz_rc _val( const size_t idx, V& val )
        {
                if ( idx < 0 || idx >= this->m_val.size() )
                        return AXZ_ERROR_OUT_OF_RANGE;

                return Internal::getVal( this->m_val[idx], val, _AxzBool2Type<isDict>() );
        }

        template<class V, bool isDict>
        axz_rc _steal( const size_t idx, V& val )
        {
                if ( idx < 0 || idx >= this->m_val.size() )
                        return AXZ_ERROR_OUT_OF_RANGE;

                return Internal::stealVal( this->m_val[idx], val, _AxzBool2Type<isDict>() );
        }
};

class _AxzObject final: public _AxzTVal< AxzDictType::OBJECT, axz_dict_object >
{
public:
        _AxzObject( const axz_dict_object& val = {} ): _AxzTVal( val )        {}
        _AxzObject( axz_dict_object&& val ): _AxzTVal( std::move( val ) )     {}

        virtual axz_rc val( const axz_wstring& key, AxzDict& val ) override			{ return this->_val<AxzDict, true>( key, val ); }
        virtual axz_rc val( const axz_wstring& key, double& val ) override			{ return this->_val<double, false>( key, val ); }
        virtual axz_rc val( const axz_wstring& key, int32_t& val ) override			{ return this->_val<int32_t, false>( key, val ); }
        virtual axz_rc val( const axz_wstring& key, bool& val ) override			{ return this->_val<bool, false>( key, val ); }
        virtual axz_rc val( const axz_wstring& key, axz_wstring& val )	override                { return this->_val<axz_wstring, false>( key, val ); }
        virtual axz_rc val( const axz_wstring& key, axz_bytes& val ) override                   { return this->_val<axz_bytes, false>( key, val ); }
        virtual const axz_dict_object& objectVal() const override                                      { return this->m_val; }
        virtual axz_rc steal( const axz_wstring& key, AxzDict& val ) override			{ return this->_steal<AxzDict, true>( key, val ); }
        virtual axz_rc steal( const axz_wstring& key, double& val ) override			{ return this->_steal<double, false>( key, val ); }
        virtual axz_rc steal( const axz_wstring& key, int32_t& val ) override			{ return this->_steal<int32_t, false>( key, val ); }
        virtual axz_rc steal( const axz_wstring& key, bool& val ) override				{ return this->_steal<bool, false>( key, val ); }
        virtual axz_rc steal( const axz_wstring& key, axz_wstring& val ) override		{ return this->_steal<axz_wstring, false>( key, val ); }
        virtual axz_rc steal( const axz_wstring& key, axz_bytes& val ) override			{ return this->_steal<axz_bytes, false>( key, val ); }

        virtual size_t size() const override	                                    { return this->m_val.size(); }
        virtual void clear() override			                                    { this->m_val.clear(); }

        virtual axz_rc add( const AxzDict& val ) override
        {
                if ( !val.isObject() )
                {
                        return AXZ_ERROR_INVALID_INPUT;
                }

                auto keys = val.keys();
                for( auto key: keys )
                {
                        this->m_val.emplace( key, val[key] );
                }
                return AXZ_OK;
        }

        virtual axz_rc add( AxzDict&& val )	override
        {
                if ( !val.isObject() )
                {
                        return AXZ_ERROR_INVALID_INPUT;
                }

                auto keys = val.keys();
                for( auto key: keys )
                {
                        this->m_val.emplace( key, std::move( val[key] ) );
                }
                return AXZ_OK;
        }

        virtual axz_rc add( const axz_wstring& key, const AxzDict& val ) override
        {
                auto found = this->m_val.find( key );
                if ( found == this->m_val.end() )
                {
                        this->m_val.emplace( key, val );
                        return AXZ_OK;
                }
                found->second = val;
                return AXZ_OK_REPLACED;
        }

        virtual axz_rc add( const axz_wstring& key, AxzDict&& val ) override
        {
                auto found = this->m_val.find( key );
                if ( found == this->m_val.end() )
                {
                        this->m_val.emplace( key, std::move( val ) );
                        return AXZ_OK;
                }
                found->second = std::move( val );
                return AXZ_OK_REPLACED;
        }

        virtual axz_rc remove( const axz_wstring& key ) override
        {
                this->m_val.erase( key );
                return AXZ_OK;
        }

        virtual axz_rc contain( const axz_wstring& key ) override
        {
                auto found = this->m_val.find( key );
                return ( found != this->m_val.end() )? AXZ_OK: AXZ_ERROR_NOT_FOUND;
        }

        virtual axz_rc contain( const axz_wstring& key, const AxzDictType type ) override
        {
                auto found = this->m_val.find( key );
                if ( found == this->m_val.end() )
                        return AXZ_ERROR_NOT_FOUND;

                return found->second.isType( type )? AXZ_OK: AXZ_ERROR_NOT_FOUND;
        }

        AxzDict& at( const axz_wstring& key ) override
        {
                auto found = this->m_val.find( key );
                assert( found != this->m_val.end() );

                return found->second;
        }

private:

        template<class V, bool isDict>
        axz_rc _val( const axz_wstring& key, V& val )
        {
                auto found = this->m_val.find( key );
                if ( found == this->m_val.end() )
                        return AXZ_ERROR_NOT_FOUND;

                return Internal::getVal( found->second, val, _AxzBool2Type<isDict>() );
        }

        template<class V, bool isDict>
        axz_rc _steal( const axz_wstring& key, V& val )
        {
                auto found = this->m_val.find( key );
                if ( found == this->m_val.end() )
                        return AXZ_ERROR_NOT_FOUND;

                return Internal::stealVal( found->second, val, _AxzBool2Type<isDict>() );
        }

        friend class _AxzDot;
};

class _AxzCallable final: public _AxzDicVal
{
public:
        _AxzCallable( const axz_dict_callable& val = nullptr ): m_val( val ) {}

        AxzDictType type() const override								        { return AxzDictType::CALLABLE; }
        virtual bool isType( const AxzDictType type ) const override	        { return ( AxzDictType::CALLABLE == type ); }
        axz_dict_callable& data() { return this->m_val; }
        bool equals(const _AxzDicVal& other) const override { return false; }
        virtual axz_rc call( AxzDict&& in_val, AxzDict& out_val ) override
        {
                out_val = this->m_val( std::move( in_val ) );
                return AXZ_OK;
        }

        virtual axz_rc step( axz_shared_dict_stepper stepper ) override
        {
                stepper->step( axz_wstring( L"callable object" ) );
                return AXZ_OK;
        }

private:
        axz_dict_callable m_val;
};

/* This class is really hot - it makes friend with both AxzDict and _AxzObject !!!*/
class _AxzDot
{
public:
        template<class V, bool isDict, bool steal>
        static axz_rc doDot( std::shared_ptr<_AxzDicVal> p_root, const axz_wstring& key_list, V& val )
        {
                if ( key_list.empty() )
                {
                        return AXZ_ERROR_INVALID_INPUT;
                }

                axz_wchar* temp = nullptr;
                axz_wchar* key_string = new axz_wchar[key_list.size() + 1]{'\0'};
                if ( !key_string )
                {
                        return AXZ_ERROR_MEMORY_DRAINED;
                }
                memcpy( key_string, key_list.c_str(), key_list.size() * sizeof( axz_wchar ) );


                std::shared_ptr<_AxzDicVal> pre_object = p_root;
                std::shared_ptr<_AxzDicVal> object = p_root;
                axz_wchar* p_tok = wcstok( key_string, L".", &temp );
                axz_wchar* last_tok = p_tok;

                while ( p_tok && object->isType( AxzDictType::OBJECT ) )
                {
                        auto u_object = ( _AxzObject* )object.get();	// get underlined object
                        auto found = u_object->m_val.find( p_tok );
                        if ( found == u_object->m_val.end() )
                        {
                                object.reset();
                                break;
                        }
                        pre_object = object;
                        object = found->second.m_val;
                        last_tok = p_tok;
                        p_tok = wcstok( nullptr, L".", &temp );
                }

                axz_rc rc = AXZ_ERROR_NOT_FOUND;
                if ( !temp && object )
                {
                        AxzDict temp_dict( object );
                        if ( steal )
                        {
                                Internal::stealVal( temp_dict, val, _AxzBool2Type<isDict>() );
                                (( _AxzObject* )pre_object.get())->at( last_tok ).drop();
                        }
                        else
                        {
                                Internal::getVal( temp_dict, val, _AxzBool2Type<isDict>() );
                        }
                        rc = AXZ_OK;
                }

                if ( key_string )
                {
                        delete[] key_string;
                }
                return rc;
        }
};


AxzDict::AxzDict() noexcept							    : m_val( _AxzDicDefault::nullVal ) {}
AxzDict::AxzDict( std::nullptr_t ) noexcept			    : m_val( _AxzDicDefault::nullVal ) {}
AxzDict::AxzDict( double val )						    : m_val( std::make_shared<_AxzDouble>( val ) ) {}
AxzDict::AxzDict( int32_t val )							: m_val( std::make_shared<_AxzInt>( val ) ) {}
AxzDict::AxzDict( bool val )							: m_val( val? _AxzDicDefault::trueVal: _AxzDicDefault::falseVal ) {}
AxzDict::AxzDict( const axz_wstring& val )				: m_val( std::make_shared<_AxzString>( val ) ) {}
AxzDict::AxzDict( axz_wstring&& val )					: m_val( std::make_shared<_AxzString>( std::move( val ) ) ) {}
AxzDict::AxzDict( const axz_wchar* val )				: m_val( std::make_shared<_AxzString>( val ) ) {}
AxzDict::AxzDict( const axz_bytes& val )				: m_val( std::make_shared<_AxzBytes>( val ) ) {}
AxzDict::AxzDict( axz_bytes&& val )					    : m_val( std::make_shared<_AxzBytes>( std::move( val ) ) ) {}
AxzDict::AxzDict( const axz_dict_array& vals )	        : m_val( std::make_shared<_AxzArray>( vals ) ) {}
AxzDict::AxzDict( axz_dict_array&& vals )		        : m_val( std::make_shared<_AxzArray>( std::move( vals ) ) ) {}
AxzDict::AxzDict( const axz_dict_object& vals )	        : m_val( std::make_shared<_AxzObject>( vals ) ) {}
AxzDict::AxzDict( axz_dict_object&& vals )		        : m_val( std::make_shared<_AxzObject>( std::move( vals ) ) ) {}
AxzDict::AxzDict( const axz_dict_callable& val )		: m_val( std::make_shared<_AxzCallable>( val ) ) {}

AxzDict::AxzDict( const AxzDict& val )
{
        this->_set( val );
}

AxzDict::AxzDict( AxzDict&& val )
{
        this->_set( std::move( val ) );
}

AxzDict::AxzDict( AxzDictType type ) noexcept
{
        this->become( type );
}

AxzDict& AxzDict::operator=( const AxzDict& other )
{
        this->_set( other );
        return *this;
}

AxzDict& AxzDict::operator=( AxzDict&& other )
{
        this->_set( std::move( other ) );
        return *this;
}

AxzDict& AxzDict::operator=( std::nullptr_t )
{
        this->m_val = _AxzDicDefault::nullVal;
        return *this;
}

AxzDict& AxzDict::operator=( double val )
{
        this->m_val = std::make_shared<_AxzDouble>( val );
        return *this;
}

AxzDict& AxzDict::operator=( int32_t val )
{
        this->m_val = std::make_shared<_AxzInt>( val );
        return *this;
}

AxzDict& AxzDict::operator=( bool val )
{
        this->m_val = std::make_shared<_AxzBool>( val );
        return *this;
}

AxzDict& AxzDict::operator=( const axz_wstring& val )
{
        this->m_val = std::make_shared<_AxzString>( val );
        return *this;
}

AxzDict& AxzDict::operator=( axz_wstring&& val )
{
        this->m_val = std::make_shared<_AxzString>( std::move( val ) );
        return *this;
}

AxzDict& AxzDict::operator=( const axz_wchar* val )
{
        this->m_val = std::make_shared<_AxzString>( val );
        return *this;
}

AxzDict& AxzDict::operator=( const axz_bytes& val )
{
        this->m_val = std::make_shared<_AxzBytes>( val );
        return *this;
}

AxzDict& AxzDict::operator=( axz_bytes&& val )
{
        this->m_val = std::make_shared<_AxzBytes>( std::move( val ) );
        return *this;
}

AxzDict& AxzDict::operator=( const axz_dict_object& other )
{
        this->m_val = std::make_shared<_AxzObject>( other );
        return *this;
}

AxzDict& AxzDict::operator=( axz_dict_object&& other )
{
        this->m_val = std::make_shared<_AxzObject>( std::move( other ) );
        return *this;
}

AxzDict& AxzDict::operator=( const axz_dict_array& other )
{
        this->m_val = std::make_shared<_AxzArray>( other );
        return *this;
}

AxzDict& AxzDict::operator=( axz_dict_array&& other )
{
        this->m_val = std::make_shared<_AxzArray>( std::move( other ) );
        return *this;
}

AxzDict& AxzDict::operator=( const axz_dict_callable& other )
{
        this->m_val = std::make_shared<_AxzCallable>( other );
        return *this;
}


AxzDictType AxzDict::type() const	                                { return this->m_val->type(); }
bool AxzDict::isType( const AxzDictType type ) const                { return this->m_val->isType( type ); }
bool AxzDict::isNull()	const	                                    { return this->m_val->isType( AxzDictType::NUL ); }
bool AxzDict::isNumber() const	                                    { return this->m_val->isType( AxzDictType::NUMBER ); }
bool AxzDict::isString() const	                                    { return this->m_val->isType( AxzDictType::STRING ); }
bool AxzDict::isBytes() const	                                    { return this->m_val->isType( AxzDictType::BYTES ); }
bool AxzDict::isArray() const	                                    { return this->m_val->isType( AxzDictType::ARRAY ); }
bool AxzDict::isObject() const	                                    { return this->m_val->isType( AxzDictType::OBJECT ); }
bool AxzDict::isCallable() 	const									{ return this->m_val->isType( AxzDictType::CALLABLE ); }

axz_rc AxzDict::val( double& val ) const                    		{ return this->m_val->val( val ); }
axz_rc AxzDict::val( int32_t& val ) const                  			{ return this->m_val->val( val ); }
axz_rc AxzDict::val( bool& val ) const			                    { return this->m_val->val( val ); }
axz_rc AxzDict::val( axz_wstring& val ) const                   	{ return this->m_val->val( val ); }
axz_rc AxzDict::val( axz_bytes& val ) const		                    { return this->m_val->val( val ); }

axz_rc AxzDict::steal( double& val )
{
        axz_rc rc = this->m_val->steal( val );
        if ( AXZ_SUCCESS( rc ) )		this->drop();
        return rc;
}

axz_rc AxzDict::steal( int32_t& val )
{
        axz_rc rc = this->m_val->steal( val );
        if ( AXZ_SUCCESS( rc ) )		this->drop();
        return rc;
}

axz_rc AxzDict::steal( bool& val )
{
        axz_rc rc = this->m_val->steal( val );
        if ( AXZ_SUCCESS( rc ) )		this->drop();
        return rc;
}

axz_rc AxzDict::steal( axz_wstring& val )								{ return this->m_val->steal( val ); }
axz_rc AxzDict::steal( axz_bytes& val )									{ return this->m_val->steal( val ); }

axz_rc AxzDict::val( const axz_wstring& key, AxzDict& val ) const		{ return this->m_val->val( key, val ); }
axz_rc AxzDict::val( const axz_wstring& key, double& val ) const		{ return this->m_val->val( key, val ); }
axz_rc AxzDict::val( const axz_wstring& key, int32_t& val )	const		{ return this->m_val->val( key, val ); }
axz_rc AxzDict::val( const axz_wstring& key, bool& val )	const		{ return this->m_val->val( key, val ); }
axz_rc AxzDict::val( const axz_wstring& key, axz_wstring& val ) const	{ return this->m_val->val( key, val ); }
axz_rc AxzDict::val( const axz_wstring& key, axz_bytes& val )	const	{ return this->m_val->val( key, val ); }

axz_rc AxzDict::dotVal( const axz_wstring& key_list, AxzDict& val ) const		{ return _AxzDot::doDot<AxzDict, true, false>( this->m_val, key_list, val ); }
axz_rc AxzDict::dotVal( const axz_wstring& key_list, double& val ) const		{ return _AxzDot::doDot<double, false, false>( this->m_val, key_list, val ); }
axz_rc AxzDict::dotVal( const axz_wstring& key_list, int32_t& val ) const		{ return _AxzDot::doDot<int32_t, false, false>( this->m_val, key_list, val ); }
axz_rc AxzDict::dotVal( const axz_wstring& key_list, bool& val ) const			{ return _AxzDot::doDot<bool, false, false>( this->m_val, key_list, val ); }
axz_rc AxzDict::dotVal( const axz_wstring& key_list, axz_wstring& val ) const	{ return _AxzDot::doDot<axz_wstring, false, false>( this->m_val, key_list, val ); }
axz_rc AxzDict::dotVal( const axz_wstring& key_list, axz_bytes& val ) const		{ return _AxzDot::doDot<axz_bytes, false, false>( this->m_val, key_list, val ); }

axz_rc AxzDict::steal( const axz_wstring& key, AxzDict& val )					{ return this->m_val->steal( key, val ); }
axz_rc AxzDict::steal( const axz_wstring& key, double& val )					{ return this->m_val->steal( key, val ); }
axz_rc AxzDict::steal( const axz_wstring& key, int32_t& val )					{ return this->m_val->steal( key, val ); }
axz_rc AxzDict::steal( const axz_wstring& key, bool& val )						{ return this->m_val->steal( key, val ); }
axz_rc AxzDict::steal( const axz_wstring& key, axz_wstring& val )				{ return this->m_val->steal( key, val ); }
axz_rc AxzDict::steal( const axz_wstring& key, axz_bytes& val )					{ return this->m_val->steal( key, val ); }

axz_rc AxzDict::dotSteal( const axz_wstring& key_list, AxzDict& val )		{ return _AxzDot::doDot<AxzDict, true, true>( this->m_val, key_list, val ); }
axz_rc AxzDict::dotSteal( const axz_wstring& key_list, double& val )		{ return _AxzDot::doDot<double, false, true>( this->m_val, key_list, val ); }
axz_rc AxzDict::dotSteal( const axz_wstring& key_list, int32_t& val )		{ return _AxzDot::doDot<int32_t, false, true>( this->m_val, key_list, val ); }
axz_rc AxzDict::dotSteal( const axz_wstring& key_list, bool& val )			{ return _AxzDot::doDot<bool, false, true>( this->m_val, key_list, val ); }
axz_rc AxzDict::dotSteal( const axz_wstring& key_list, axz_wstring& val )	{ return _AxzDot::doDot<axz_wstring, false, true>( this->m_val, key_list, val ); }
axz_rc AxzDict::dotSteal( const axz_wstring& key_list, axz_bytes& val )		{ return _AxzDot::doDot<axz_bytes, false, true>( this->m_val, key_list, val ); }

axz_rc AxzDict::val( const size_t idx, AxzDict& val ) const		    { return this->m_val->val( idx, val ); }
axz_rc AxzDict::val( const size_t idx, double& val ) const			{ return this->m_val->val( idx, val ); }
axz_rc AxzDict::val( const size_t idx, int32_t& val ) const			{ return this->m_val->val( idx, val ); }
axz_rc AxzDict::val( const size_t idx, bool& val ) const			{ return this->m_val->val( idx, val ); }
axz_rc AxzDict::val( const size_t idx, axz_wstring& val ) const		{ return this->m_val->val( idx, val ); }
axz_rc AxzDict::val( const size_t idx, axz_bytes& val ) const		{ return this->m_val->val( idx, val ); }

axz_rc AxzDict::steal( const size_t idx, AxzDict& val )				{ return this->m_val->steal( idx, val ); }
axz_rc AxzDict::steal( const size_t idx, double& val )				{ return this->m_val->steal( idx, val ); }
axz_rc AxzDict::steal( const size_t idx, int32_t& val )				{ return this->m_val->steal( idx, val ); }
axz_rc AxzDict::steal( const size_t idx, bool& val )				{ return this->m_val->steal( idx, val ); }
axz_rc AxzDict::steal( const size_t idx, axz_wstring& val )			{ return this->m_val->steal( idx, val ); }
axz_rc AxzDict::steal( const size_t idx, axz_bytes& val )			{ return this->m_val->steal( idx, val ); }

axz_rc AxzDict::call( AxzDict&& in_val, AxzDict& out_val ) const	{ return this->m_val->call( std::move( in_val ), out_val ); }
AxzDict AxzDict::operator()( AxzDict&& val ) const
{
        AxzDict res;
        axz_rc rc = AXZ_OK;
        if ( AXZ_FAILED( rc = this->m_val->call( std::move( val ), res ) ) )
        {
                return axz_dict_object{ { L"code", rc } };
        }
        return res;
}

double 		AxzDict::numberVal() const                       		{ return this->m_val->numberVal(); }
int32_t		AxzDict::intVal() const			                        { return this->m_val->intVal(); }
bool 		AxzDict::boolVal() const                                        { return this->m_val->boolVal(); }
axz_wstring AxzDict::stringVal() const                                          { return this->m_val->stringVal(); }
axz_bytes 	AxzDict::bytesVal() const		                        { return this->m_val->bytesVal(); }
size_t 		AxzDict::size() const			                        { return this->m_val->size(); }

const AxzDict& AxzDict::operator[]( const size_t idx ) const
{
        assert( this->m_val->type() == AxzDictType::ARRAY );
  return this->m_val->at( idx );
}

bool AxzDict::operator==(const AxzDict &other) const
{
  if(other.m_val == this->m_val) { return true; }
  if(other.type() != this->type()) { return false; }
  return other.m_val->equals(*this->m_val);
}

bool AxzDict::operator!=(const AxzDict &other) const
{
  return !(this->operator==(other));
}

AxzDict& AxzDict::operator[]( const size_t idx )
{
  assert( this->m_val->type() == AxzDictType::ARRAY );
        return this->m_val->at( idx );
}

const AxzDict& AxzDict::operator[]( const axz_wstring& key ) const
{
        assert( this->m_val->type() == AxzDictType::OBJECT );
        return this->m_val->at( key );
}

AxzDict& AxzDict::operator[]( const axz_wstring& key )
{
        assert( this->m_val->type() == AxzDictType::OBJECT );
        return this->m_val->at( key );
}

axz_rc AxzDict::add( const AxzDict& val )		                        { return this->m_val->add( val ); }
axz_rc AxzDict::add( AxzDict&& val )			                        { return this->m_val->add( std::move( val ) ); }
axz_rc AxzDict::add( const axz_wstring& key, const AxzDict& val )	    { return this->m_val->add( key, val ); }
axz_rc AxzDict::add( const axz_wstring& key, AxzDict&& val )		    { return this->m_val->add( key, std::move( val ) ); }

void AxzDict::clear()								                    { this->m_val->clear(); }
axz_rc AxzDict::remove( const size_t idx )			                    { return this->m_val->remove( idx ); }
axz_rc AxzDict::remove( const axz_wstring& key )		                { return this->m_val->remove( key ); }
axz_rc AxzDict::contain( const axz_wstring& key ) const	                { return this->m_val->contain( key ); }
axz_rc AxzDict::contain( const axz_wstring& key, const AxzDictType type ) const { return this->m_val->contain( key, type ); }

axz_dict_keys AxzDict::keys() const
{
        axz_dict_keys keys;
        if( this->m_val->isType( AxzDictType::OBJECT ) )
        {
                auto items = ( ( _AxzObject* )this->m_val.get() )->data();
                // populate our keys
                for( auto item : items )
                {
                        keys.emplace( item.first );
                }
        }
        return keys;
}

void AxzDict::become( AxzDictType type )
{
        switch ( type )
        {
        case AxzDictType::NUL:
                this->m_val = std::make_shared<_AxzNull>();
                break;
        case AxzDictType::BOOL:
                this->m_val = std::make_shared<_AxzBool>();
                break;
        case AxzDictType::NUMBER:
                this->m_val = std::make_shared<_AxzDouble>( 0. );
                break;
        case AxzDictType::STRING:
                this->m_val = std::make_shared<_AxzString>();
                break;
        case AxzDictType::BYTES:
                this->m_val = std::make_shared<_AxzBytes>();
                break;
        case AxzDictType::ARRAY:
                this->m_val = std::make_shared<_AxzArray>();
                break;
        case AxzDictType::OBJECT:
                this->m_val = std::make_shared<_AxzObject>();
                break;
        case AxzDictType::CALLABLE:
                this->m_val = std::make_shared<_AxzCallable>();
                break;
        default:
                this->m_val = std::make_shared<_AxzNull>();
                break;
        }
}

void AxzDict::drop()
{
        this->m_val = _AxzDicDefault::nullVal;
}

axz_rc AxzDict::step( std::shared_ptr<AxzDictStepper> stepper ) const
{
    if ( !stepper )
    {
        return AXZ_ERROR_NOT_READY;
    }
    return this->m_val->step( stepper );
}

void AxzDict::_set( const AxzDict& val )
{
        switch ( val.m_val->type() )
        {
        case AxzDictType::NUL:
                this->m_val = _AxzDicDefault::nullVal;
                break;
        case AxzDictType::BOOL:
                this->m_val = std::make_shared<_AxzBool>( val.m_val->boolVal() );
                break;
        case AxzDictType::NUMBER:
        {
                if ( typeid( *val.m_val.get() ) == typeid (_AxzInt ) )
                        this->m_val = std::make_shared<_AxzInt>( val.m_val->intVal() );
                else
                        this->m_val = std::make_shared<_AxzDouble>( val.m_val->numberVal() );
                break;
        }
        case AxzDictType::STRING:
                this->m_val = std::make_shared<_AxzString>( ( ( _AxzString* )val.m_val.get() )->data() );
                break;
        case AxzDictType::BYTES:
                this->m_val = std::make_shared<_AxzBytes>( ( ( _AxzBytes* )val.m_val.get() )->data() );
                break;
        case AxzDictType::ARRAY:
                this->m_val = std::make_shared<_AxzArray>( ( ( _AxzArray* )val.m_val.get() )->data() );
                break;
        case AxzDictType::OBJECT:
                this->m_val = std::make_shared<_AxzObject>( ( ( _AxzObject* )val.m_val.get() )->data() );
                break;
        case AxzDictType::CALLABLE:
                this->m_val = std::make_shared<_AxzCallable>( ( ( _AxzCallable* )val.m_val.get() )->data() );
                break;
        default:
                break;
        }
}

void AxzDict::_set( AxzDict&& val )
{
        switch ( val.m_val->type() )
        {
        case AxzDictType::NUL:
                this->m_val = _AxzDicDefault::nullVal;
                break;
        case AxzDictType::BOOL:
                this->m_val = std::make_shared<_AxzBool>( val.m_val->boolVal() );
                break;
        case AxzDictType::NUMBER:
        {
                if ( typeid( *val.m_val.get() ) == typeid (_AxzInt ) )
                        this->m_val = std::make_shared<_AxzInt>( val.m_val->intVal() );
                else
                        this->m_val = std::make_shared<_AxzDouble>( val.m_val->numberVal() );
                break;
        }
        case AxzDictType::STRING:
                this->m_val = std::make_shared<_AxzString>( std::move( ( ( _AxzString* )val.m_val.get() )->data() ) );
                break;
        case AxzDictType::BYTES:
                this->m_val = std::make_shared<_AxzBytes>( std::move( ( ( _AxzBytes* )val.m_val.get() )->data() ) );
                break;
        case AxzDictType::ARRAY:
                this->m_val = std::make_shared<_AxzArray>( std::move( ( ( _AxzArray* )val.m_val.get() )->data() ) );
                break;
        case AxzDictType::OBJECT:
                this->m_val = std::make_shared<_AxzObject>( std::move( ( ( _AxzObject* )val.m_val.get() )->data() ) );
                break;
        case AxzDictType::CALLABLE:
                this->m_val = std::make_shared<_AxzCallable>( ( ( _AxzCallable* )val.m_val.get() )->data() );
                break;
        default:
                break;
        }
}

