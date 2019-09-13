/**
 * @file myxml.h
 * @brief XML utilities Header
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * 2008
 */

#ifndef MYXML_H
#define MYXML_H

#include <libxml/tree.h>
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <boost/lexical_cast.hpp>

#include "base64.h"

namespace dominiqs {

typedef std::map<std::string, std::string> AttributeMap;

/**
 * C++ Wrapper aroung libxml xmlNodePtr
 * Can be used as iterator and navigate through the DOM tree
 */

class XmlNode
{
public:
	XmlNode(xmlNodePtr n = 0) : node(n) {}
	bool isNull() const { return !node; }
	// get data
	std::string name() const;
	std::string attr(const std::string& name) const;
	std::string data() const;
	// navigation
	XmlNode parent() const;
	XmlNode firstChild() const;
	XmlNode next() const;
	XmlNode prev() const;
	XmlNode next(int skip) const;
	XmlNode prev(int skip) const;
	XmlNode operator[](const std::string& name) const;
	XmlNode operator[](int idx) const;
	// modify
	void set(const std::string& content, bool checkSpecialChars = false);
	void unlink();
	XmlNode addChild(const std::string& name, const AttributeMap& attrs = AttributeMap());
	XmlNode addChild(const std::string& name, const std::string& content, const AttributeMap& attrs = AttributeMap());
protected:
	xmlNodePtr node;
};

/**
 * C++ Wrapper aroung libxml xmlDocPtr
 * Represents an XML document
 */

class XmlDoc
{
public:
	XmlDoc(xmlDocPtr d = 0) : doc(d) {}
	~XmlDoc();
	// read/write to file
	void read(const std::string& filename);
	void write(const std::string& filename) const;
	// get/set root
	XmlNode getRoot();
	XmlNode setRoot(const std::string& name, const AttributeMap& attrs = AttributeMap());
	// xpath query
	void xpath(const std::string& xpathExpr, std::vector<XmlNode>& res) const;
	XmlNode xpath(const std::string& xpathExpr) const;
protected:
	void free();
	xmlDocPtr doc;
private:
	// forbid copy constructor and assignment
	XmlDoc& operator=(const XmlDoc& rhs);
	XmlDoc(const XmlDoc& rhs);
};

/**
 * Read data content of node @param node and convert it to an element of type @tparam T
 * @return the converted value or @param defValue if node is empty or conversion fails
 */

template<class T>
T getTextContent(const XmlNode& node, const T& defValue)
{
	if (node.isNull()) return defValue;
	try  { return boost::lexical_cast<T>(node.data()); }
	catch(boost::bad_lexical_cast&) { return defValue; }
}

/**
 * Read data content of node @param node and convert it to an element of type @tparam T
 *
 * The text data of the node is interpreted as a base64 encoding of binary data
 * representation of an object of type T
 * @return the converted value or @param defValue if node is empty or conversion fails
 */

template<class T>
T getBase64Content(const XmlNode& node, const T& defValue)
{
	if (node.isNull()) return defValue;
	try
	{
		T value;
		b64_decode(node.data(), value);
		return value;
	}
	catch(std::runtime_error&) { return defValue; }
}

} // namespace dominiqs

#endif /* MYXML_H */
