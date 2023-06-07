//
// Created by luo-zeqi on 2023/6/2.
//

#ifndef LIBXDOC_DOCUMENT_H
#define LIBXDOC_DOCUMENT_H

#include "rbtree.h"
#include "llist.h"

#include <string>

enum XError {
  xNoErr,
  xErrMemAlloc,
  xErrBadFile,
  xErrEmptyFile,
  xErrIncompleteDoc,
  xErrParse,
};

enum XNodeType {
  xNodeTypeNone,
  xNodeTypeElement,
  xNodeTypeComment,
  xNodeTypeText,
};

///@brief 由于 xml 其结构与 树 的结构同理， 所以我们可以将文档中
/// 任何东西都映射成树中的节点, 这里的节点是一个抽象的，由于仅元素
/// 类型的节点才拥有子节点，所以节点无需拥有子节点字段。
struct XNode {
  llnode_t    llnode;
  XNodeType   type;
  std::string txt;

  XNode(XNodeType t) {
    type = t;
  }

  ///@brief 设置节点文本
  void setTxt(const char *t, int len = -1);
  void setTxt(const std::string& t);

  ///@brief 获取上一个同级节点
  XNode *prev();
  ///@brief 获取下一个同级节点
  XNode *next();
};

struct XAttribute {
  rbnode_t rbnode;
  std::string key;
  std::string val;
};

///@brief 注释与文本节点无特别之处，直接使用 XNode 即可.
typedef XNode XComments;
typedef XNode XText;

///@brief 代表 xml 文档中的一个元素
/// 使用重叠结构方式模拟继承(由于内部使用了c实现的数据结构，
/// 如果还是使用面向对象强转时会导致对象丢失数据)
struct XElement {
  XNode    node;
  XNode    children;
  rbtree_t attrs;

  XElement();
  ~XElement();

  std::string& name() {
    return node.txt;
  }

  /// @brief 设置元素名
  void setName(const char *name, int len = -1) {
    node.setTxt(name, len);
  }
  void setName(const std::string& name) {
    node.setTxt(name.c_str(), (int) name.length());
  }

  ///@brief 添加属性，需要提供属性名称.
  XAttribute *addAttr(const char *key, int len = -1);
  XAttribute *addAttr(const std::string& key);
  ///@brief 通过属性名称查找.
  XAttribute *operator [] (const char *key) const;
  XAttribute *operator [] (const std::string& key) const;
  XAttribute *findAttr(const char *key) const;
  XAttribute *findAttr(const std::string& key) const;

  ///@brief 添加一个子节点，共3种不同的节点类型.
  XComments *addChildComment();
  XText     *addChildText();
  XElement  *addChildElement();

  ///@brief 获取首个或最后一个子元素.
  /// 若没有则返回 null.
  XElement *first();
  XElement *last();

  ///@brief 获取前一个或后一个同级元素
  /// 若没则返回 null.
  XElement *prev();
  XElement *next();
};

/// @brief: 遍历所有子元素.
/// @note: 在遍历期间请勿改变
#define ELEMENT_FOREACH(child, ele)        \
  for (XElement *(child) = (ele)->first(); \
       (child) != nullptr;                 \
       (child) = (child)->next()           \
  )

class XDocument {
public:
  XDocument();
  XDocument(const std::string& path);
  ~XDocument();

  bool load(const std::string& path);
  bool save(const std::string& path = {});
  XError      error();
  std::string errorText();

  XElement *root();

  void setRoot(XElement &&root);

private:
  std::string filePath_;
  XError      error_;
  std::string errtxt_;

  std::string hversion_;
  std::string hencoding_;
  std::string hstandalone_;

  XElement *root_;
};

#endif //LIBXDOC_DOCUMENT_H
