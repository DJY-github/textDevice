/*
*	util_treenode.h
*
*/

#pragma once

#include "source_config.h"
#include <list>

namespace util {
template<class T_Identifier, class T_Attribute>
class NodeTemplate
{
public:
	typedef std::list<NodeTemplate*>   Children;
	typedef typename Children::iterator 		   ChildrenIt;
	explicit NodeTemplate(const T_Identifier &identifier);
	virtual ~NodeTemplate(void);

	void								bind(const T_Identifier &identifier);
	inline NodeTemplate*				parent() const		{return parent_;}
	inline T_Identifier&				identifier() 		{return identifier_;}
	inline const T_Identifier&			identifier() const	{return identifier_;}
	inline T_Attribute&					attribute() 		{return attribute_;}
	inline const T_Attribute&			attribute() const	{return attribute_;}

public:
	void								addChild(NodeTemplate* child);
	void								removeChild(NodeTemplate* child);			// 只从链表中移除，不删除对象

	inline
	const Children&						children() const {return children_;}

	void								getAllSubNode(Children& list_subnode) const;

private:
	NodeTemplate*						parent_;
	T_Identifier						identifier_;
	T_Attribute							attribute_;
	Children							children_;

};

template<class T_Identifier, class T_Attribute>
NodeTemplate<T_Identifier, T_Attribute>::NodeTemplate(const T_Identifier &identifier)
{
	parent_	= nullptr;
	bind(identifier);
}

template<class T_Identifier, class T_Attribute>
NodeTemplate<T_Identifier, T_Attribute>::~NodeTemplate(void)
{
	ChildrenIt it;
	for(it=children_.begin(); it!=children_.end(); it++)
	{
		delete (*it);
		(*it) = nullptr;
	}
	children_.clear();
}


template<class T_Identifier, class T_Attribute>
void NodeTemplate<T_Identifier, T_Attribute>::bind(const T_Identifier &identifier)
{
	identifier_ = identifier;
}

template<class T_Identifier, class T_Attribute>
void NodeTemplate<T_Identifier, T_Attribute>::addChild(NodeTemplate* child)
{
	children_.push_back(child);
	child->parent_	= this;
}
template<class T_Identifier, class T_Attribute>
void NodeTemplate<T_Identifier, T_Attribute>::removeChild(NodeTemplate* child)
{
	ChildrenIt it = children_.begin();
	for(; it != children_.end(); it++)
	{
		if((*it) == child)
		{
			children_.erase(it);
			break;
		}
	}
}

template<class T_Identifier, class T_Attribute>
void	NodeTemplate<T_Identifier, T_Attribute>::getAllSubNode(Children& list_subnode) const
{
	typename Children::const_iterator it;
	for(it=children_.begin(); it!=children_.end(); it++)
	{
		list_subnode.push_back(*it);
		(*it)->getAllSubNode(list_subnode);
	}

}

}   // end namespace
