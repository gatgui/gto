//-*****************************************************************************
// 
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2 of
// the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
// USA
//
//-*****************************************************************************

#ifndef _GtoContainer_PropertyContainer_h_
#define _GtoContainer_PropertyContainer_h_

#include <GtoContainer/Protocol.h>
#include <GtoContainer/StdProperties.h>
#include <GtoContainer/Component.h>
#include <GtoContainer/Exception.h>
#include <string>

namespace GtoContainer {

//-*****************************************************************************
// class PropertyContainer
// Holds a list of components which in turn hold a list of properites.
// All the string needs are handled by the std::string class, which can
// explicitly construct itself in place from a const char *. Thus, no need
// for const char * versions.
class PropertyContainer
{
public:
    typedef std::vector<Component*> Components;
    
    PropertyContainer();
    PropertyContainer( const std::string &nme,
                       const Protocol &p );
    virtual ~PropertyContainer();

    //-*************************************************************************
    // ALL PROPERTY CONTAINERS MUST HAVE AT THE LEAST
    // A NAME
    // A PROTOCOL
    // A PROTOCOL VERSION
    //
    // The getters and setters are virtual so that derived classes can turn
    // them off, as a derived class would probably hard-code protocol and
    // protocol version.
    //
    // These are passive things. They don't automatically synchronize
    // with any object properties, nor do they copy. You have to do that
    // manually.
    // 
    //-*************************************************************************
    virtual std::string name() const;
    virtual void setName( const std::string &nme );
    
    virtual Protocol protocol() const;
    virtual void setProtocol( const Protocol &prot );

    // Copying
    virtual PropertyContainer* copy() const;

    // This is essentially "copy from"
    void copy( const PropertyContainer *other );
    
    // Component Management
    Components &components() { return m_components; }
    const Components &components() const { return m_components; }

    virtual void add( Component * );
    virtual void remove( Component * );
    
    // Access to properties and components
    Component *component( const std::string &name );
    const Component *component( const std::string &name ) const;
    Component *componentOf( const Property * );
    const Component *componentOf( const Property * ) const;

    bool hasComponent( Component * ) const;
    
    // Will return an existing component.
    Component *createComponent( const std::string &name,
                                bool transposeable = false );
    
    // Search for properties
    Property *find( const std::string &comp,
                    const std::string &name );

    const Property *find( const std::string &comp,
                          const std::string &name ) const;

    // Return found properties with an attempted upcast, which
    // may return NULL if the property is not that type.
    template <class T>
    T *property( const std::string &comp,
                 const std::string &name );

    template <class T>
    const T *property( const std::string &comp,
                       const std::string &name ) const;
    
    // Create or return existing property
    template <class T>
    T *createProperty( const std::string &comp,
                       const std::string &name );
    
    //  Remove property
    template <class T>
    T *removeProperty( const std::string &comp,
                       const std::string &name );

    void removeProperty( Property * );
    
    // This function will make sure that this container is "synchronized"
    // with the passed in container. That means that this container has
    // exactly the same properties as the passed in one. It does not mean
    // that it has the same data or even that the property sizes are the
    // same.
    void synchronize( const PropertyContainer * );
    
    // Concatenate (intelligently) the passed in property container's
    // data onto this one's. The default implementation is *not*
    // intelligent about the concatenation, so you should certainly
    // override it.
    virtual void concatenate( const PropertyContainer * );
    
    // Should be archived?
    bool isPersistent() const;

protected:
    virtual PropertyContainer* emptyContainer() const;

    //-*************************************************************************
    // DATA
    //-*************************************************************************
    std::string m_name;
    Protocol m_protocol;

private:
    Components m_components;
};

//-*****************************************************************************
template <class T>
T*
PropertyContainer::property( const std::string &comp, 
                             const std::string &name )
{
    if ( Component *c = component( comp ) )
    {
        return c->property<T>( name );
    }
    
    return 0;
}

//-*****************************************************************************
template <class T>
const T*
PropertyContainer::property( const std::string &comp, 
                             const std::string &name ) const
{
    if ( const Component *c = component( comp ) )
    {
        return c->property<const T>( name );
    }
	
    return 0;
}

//-*****************************************************************************
template <class T>
T*
PropertyContainer::createProperty( const std::string &comp, 
                                   const std::string &name )
{
    if ( Component *c = createComponent( comp ) )
    {
        if ( T *p = c->property<T>( name ) )
        {
            return p;
        }
        else
        {
            T *pp = new T( name );
            c->add( pp );
            return pp;
        }
    }

    throw UnexpectedExc(
        ", PropertyContainer::createComponent() failed" );
}

//-*****************************************************************************
template <class T>
T*
PropertyContainer::removeProperty( const std::string &comp, 
                                   const std::string &name )
{
    if ( Component *c = component( comp ) )
    {
        if ( T *p = c->property<T>( name ) )
        {
            c->remove( p );
            return p;
        }
    }

    std::string msg = comp;
    msg += ".";
    msg += name;
    msg += " does not exist, remove failed.";
    throw NoPropertyExc( msg.c_str() );
}

} // End namespace GtoContainer

#endif
