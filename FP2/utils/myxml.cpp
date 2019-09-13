/**
 * @file myxml.cpp
 * @brief XML utilities Source
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * 2008
 */

#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xmlstring.h>
#include <libxml/entities.h>
#include <libxml/xpathInternals.h>

#include "myxml.h"
#include "asserter.h"

namespace dominiqs {

std::string XmlNode::name() const
{
	if (node && node->name) return std::string((const char*)node->name);
	return std::string();
}

std::string XmlNode::attr(const std::string& name) const
{
	xmlChar* data = xmlGetProp(node, (const xmlChar*)name.c_str());
	if (data) return std::string((const char*)data);
	return std::string();
}

std::string XmlNode::data() const
{
	xmlChar* data = xmlNodeGetContent(node);
	if (data) return std::string((const char*)data);
	return std::string();
}

XmlNode XmlNode::parent() const
{
	return XmlNode(node ? node->parent : 0);
}

XmlNode XmlNode::firstChild() const
{
	if (!node) return XmlNode();
	xmlNodePtr n = node->children;
	while (n && n->type != XML_ELEMENT_NODE) n = n->next;
	return XmlNode(n);
}

XmlNode XmlNode::next() const
{
	if (!node) return XmlNode();
	xmlNodePtr n = node->next;
	while (n && n->type != XML_ELEMENT_NODE) n = n->next;
	return XmlNode(n);
}

XmlNode XmlNode::prev() const
{
	if (!node) return XmlNode();
	xmlNodePtr n = node->prev;
	while (n && n->type != XML_ELEMENT_NODE) n = n->prev;
	return XmlNode(n);
}

XmlNode XmlNode::next(int skip) const
{
	DOMINIQS_ASSERT( skip > 0 );
	if (!node) return XmlNode();
	xmlNodePtr n = node->next;
	for (int i = 0; (i < skip) && n; i++)
		while (n->type != XML_ELEMENT_NODE) n = n->next;
	return XmlNode(n);
}

XmlNode XmlNode::prev(int skip) const
{
	DOMINIQS_ASSERT( skip > 0 );
	if (!node) return XmlNode();
	xmlNodePtr n = node->prev;
	for (int i = 0; (i < skip) && n; i++)
		while (n->type != XML_ELEMENT_NODE) n = n->prev;
	return XmlNode(n);
}

XmlNode XmlNode::operator[](const std::string& name) const
{
	XmlNode first = firstChild();
	while (!first.isNull())
	{
		if (first.name() == name) return first;
		first = first.next();
	}
	return XmlNode();
}

XmlNode XmlNode::operator[](int idx) const
{
	XmlNode first = firstChild();
	if (idx) first.next(idx);
	return first;
}

void XmlNode::set(const std::string& content, bool checkSpecialChars)
{
	if (!node) return;
	if (checkSpecialChars)
	{
		xmlChar* data = xmlEncodeSpecialChars(node->doc, (const xmlChar*)content.c_str());
		xmlNodeSetContent(node, data);
		xmlFree(data);
	}
	else xmlNodeSetContent(node, (const xmlChar*)content.c_str());
}

XmlNode XmlNode::addChild(const std::string& name, const AttributeMap& attrs)
{
	xmlNodePtr newNode = xmlNewChild(node, 0, (const xmlChar*)name.c_str(), 0);
	AttributeMap::const_iterator itr = attrs.begin();
	AttributeMap::const_iterator end = attrs.end();
	while (itr != end)
	{
		xmlNewProp(newNode, (const xmlChar*)(itr->first.c_str()), (const xmlChar*)(itr->second.c_str()));
		++itr;
	}
	return XmlNode(newNode);
}

XmlNode XmlNode::addChild(const std::string& name, const std::string& content, const AttributeMap& attrs)
{
	xmlNodePtr newNode = xmlNewChild(node, 0, (const xmlChar*)name.c_str(), (const xmlChar*)content.c_str());
	AttributeMap::const_iterator itr = attrs.begin();
	AttributeMap::const_iterator end = attrs.end();
	while (itr != end)
	{
		xmlNewProp(newNode, (const xmlChar*)(itr->first.c_str()), (const xmlChar*)(itr->second.c_str()));
		++itr;
	}
	return XmlNode(newNode);
}

void XmlNode::unlink()
{
	if (node)
	{
		xmlUnlinkNode(node);
		xmlFreeNode(node);
		node = 0;
	}
}

XmlDoc::~XmlDoc()
{
	if (doc) free();
}

void XmlDoc::read(const std::string& filename)
{
	if (doc) free();
	doc = xmlReadFile(filename.c_str(), 0, XML_PARSE_NOBLANKS);
}

void XmlDoc::write(const std::string& filename) const
{
	xmlSaveFormatFile(filename.c_str(), doc, 1);
}

void XmlDoc::free()
{
	xmlFreeDoc(doc);
	doc = 0;
}

XmlNode XmlDoc::getRoot()
{
	return XmlNode(xmlDocGetRootElement(doc));
}

XmlNode XmlDoc::setRoot(const std::string& name, const AttributeMap& attrs)
{
	if (doc) free();
	doc = xmlNewDoc((const xmlChar*)"1.0");
	xmlNodePtr root = xmlNewNode(0, (const xmlChar*)name.c_str());
	AttributeMap::const_iterator itr = attrs.begin();
	AttributeMap::const_iterator end = attrs.end();
	while (itr != end)
	{
		xmlNewProp(root, (const xmlChar*)(itr->first.c_str()), (const xmlChar*)(itr->second.c_str()));
		++itr;
	}
	xmlDocSetRootElement(doc, root);
	return XmlNode(root);
}

void XmlDoc::xpath(const std::string& xpathExpr, std::vector<XmlNode>& res) const
{
	xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
	xmlXPathObjectPtr obj = xmlXPathEvalExpression((const xmlChar*)xpathExpr.c_str(), ctx);
	if (obj && obj->nodesetval)
	{
		xmlNodeSetPtr set = obj->nodesetval;
		res.resize(set->nodeNr);
		for (int i = 0; i < set->nodeNr; i++) res[i] = XmlNode(set->nodeTab[i]);
	}
	else res.clear();
	xmlXPathFreeObject(obj);
	xmlXPathFreeContext(ctx);
}

XmlNode XmlDoc::xpath(const std::string& xpathExpr) const
{
	std::vector<XmlNode> res;
	xpath(xpathExpr, res);
	if (!res.empty()) return res[0];
	else return XmlNode();
}

} // namespace dominiqs
