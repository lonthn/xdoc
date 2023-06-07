//
// Created by luo-zeqi on 2023/6/5.
//

#include "document.h"

#include <memory.h>
#include <string.h>

typedef const char *ContentPtr;

struct XParser {
  ContentPtr curr;
  ContentPtr end;

  bool loadComments;

  XDocument *doc;

  bool parse();
  bool parseProlog();
  bool parseElement(XElement *ele);
  bool parseElementAttrs(XElement *ele);
  bool parseElementChildren(XElement *ele);

  bool parseComment(XElement *parent);
  bool parseText(XElement *parent);

  bool SkipBlank();
  bool MatchName(ContentPtr& b, ContentPtr& e);
  bool MatchNameStartChar();
  bool MatchNameChar();
  bool MatchRefVal(ContentPtr& b, ContentPtr& e);

  bool IsEnd();

  bool DecodeUTF8(uint32_t *unicode);
};

bool XParser::parse() {
  if (parseProlog())
    return true;

  if (SkipBlank())
    return true;

  XElement element;
  if (parseElement(&element))
    return true;

  doc->setRoot(std::move(element));
  return false;
}

bool XParser::parseProlog() {
  return false; // <?xml version="1.0" encoding="UTF-32"?>
}

bool XParser::parseElement(XElement *ele) {
  ContentPtr nbeg, nend;

  if (*(curr++) != '<') {
    return true;
  }

  if (MatchName(nbeg, nend)) return true;

  // TODO: 如果要获得更准确的错误反馈，
  //  这里还需要判断名字是否以 '空白' 或 '>' 结尾.

  ele->setName(nbeg, nend - nbeg);

  if (SkipBlank()) return true;
  if (parseElementAttrs(ele)) return true;

  // not has children node.
  if (*curr == '/') {
    if (*(++curr) == '>') {
      curr++;
      return false;
    } else {
      // TODO: 无效的符号.
      return true;
    }
  }

  assert(*curr == '>');

  curr++;

  return parseElementChildren(ele);
}

bool XParser::parseElementAttrs(XElement *ele) {
  ContentPtr nbeg, nend;

  while (*curr != '>' && *curr != '/' && curr != end) {
    if (MatchName(nbeg, nend)) return true;
    if (SkipBlank()) return true;
    
    if (*(curr++) != '=') {
      //TODO: 属性名称后面应该是 '='.
      return true;
    }

    XAttribute *attr = ele->addAttr(nbeg, (int) (nend - nbeg));
    
    if (SkipBlank()) return true;
    if (MatchRefVal(nbeg, nend)) return true;

    attr->val.assign(nbeg, (int) (nend - nbeg));

    nbeg = curr;
    if (SkipBlank()) return true;
    if (nbeg == curr && *curr != '>' && *curr != '/') {
      // TODO: 属性与属性之间需要保持一点距离才行.
      return true;
    }
  }

  return IsEnd();
}

bool XParser::parseElementChildren(XElement *ele) {
  ContentPtr nbeg, nend;
  nbeg = curr;
  while (true) {
    if (SkipBlank()) return true;

    if (*(curr++) == '<') {
      if (*curr == '/') { // 结束标签
        curr++;
        if (MatchName(nbeg, nend)) return true;
        if (memcmp(ele->name().c_str(), nbeg, ele->name().length()) != 0) {
          // TODO: 结束标签与开始标签不匹配.
          return true;
        }

        SkipBlank();

        if (*(curr)++ != '>') {
          //TODO: 结束标签不能包含除标签名以外的任何信息.
          return true;
        }

        return false;
      } else if (memcmp(curr, "!--", 3) == 0) { // 注释
        curr += 3;
        if (parseComment(ele)) return true;
        nbeg = curr;
      } else { //
        XElement *child = ele->addChildElement();
        curr--;
        if (parseElement(child)) return true;
        nbeg = curr;
      }
     } else {
      curr = nbeg;
      if (parseText(ele)) return true;
    }
  }
}

bool XParser::parseComment(XElement *parent) {
  ContentPtr nbeg, nend;
  nbeg = curr;

  while (++curr != end) {
    if (*curr == '-' && memcmp(curr, "-->", 3) == 0) {
      nend = curr;
      break;
    }
  }

  curr += 3;
  
  if (IsEnd()) return true;

  if (loadComments) {
    XNode *node = parent->addChildComment();
    node->setTxt(nbeg, (int) (nend - nbeg));
  }

  return false;
}

bool XParser::parseText(XElement *parent) {
  ContentPtr nbeg, nend;
  nbeg = curr;

  // TODO: 是否需要检验文本内容合法?.
  while (++curr != end) {
    // 解析至出现新的标签起始符,判定为文本结束.
    if (*curr == '<') {
      nend = curr;
      break;
    }
  }

  if (IsEnd()) return true;

  XNode *node = parent->addChildText();
  node->setTxt(nbeg, (int) (nend - nbeg));

  return false;
}

bool XParser::SkipBlank() {
  // 跳过空白段，包括一个或多个空格字符，回车，换行和制表符
  // S ::= (#x20 | #x9 | #xD | #xA)+
  while ((curr != end)
      && (*curr == 0x20 || *curr == 0x9 || *curr == 0xD || *curr == 0xA)) {
    curr++;
  }
  return IsEnd();
}

#define IS_LETTER(ch) ( \
  ((ch) >= 'A' && (ch) <= 'Z') ||  \
  ((ch) >= 'a' && (ch) <= 'z')     \
)

#define IS_NUMBER(ch) ( \
  ((ch) >= '0' && (ch) <= '9') \
)

bool XParser::MatchName(ContentPtr& b, ContentPtr& e) {
  b = curr;

  if (MatchNameStartChar()) return true;

  while (curr != end) {
    if (MatchNameChar())
      break;

    curr++;
  }

  if (IsEnd()) return true;

  e = curr;
  return false;
}

bool XParser::MatchNameStartChar() {
  // 匹配名字的起始字符
  // NameStartChar ::= ":" | [A-Z] | "_" | [a-z] | [#xC0-#xD6] |
  //               [#xD8-#xF6] | [#xF8-#x2FF] | [#x370-#x37D] |
  //               [#x37F-#x1FFF] | [#x200C-#x200D] | [#x2070-#x218F] |
  //               [#x2C00-#x2FEF] | [#x3001-#xD7FF] | [#xF900-#xFDCF] |
  //               [#xFDF0-#xFFFD] | [#x10000-#xEFFFF]
  if (!IS_LETTER(*curr) && *curr != ':' && *curr != '_') {
    uint32_t uc;
    if (DecodeUTF8(&uc)) return true;
    if (!((uc >= 0xD8 && uc <= 0xF6)
          || (uc >= 0xF8 && uc <= 0x2FF)
          || (uc >= 0x370 && uc <= 0x37D)
          || (uc >= 0x37F && uc <= 0x1FFF)
          || (uc >= 0x200C && uc <= 0x200D)
          || (uc >= 0x2070 && uc <= 0x218F)
          || (uc >= 0x2C00 && uc <= 0x2FEF)
          || (uc >= 0x3001 && uc <= 0xD7FF)
          || (uc >= 0xF900 && uc <= 0xFDCF)
          || (uc >= 0xFDF0 && uc <= 0xFFFD)
          || (uc >= 0x10000 && uc <= 0xEFFFF))) {
      return true;
    }
  }

  return false;
}

bool XParser::MatchNameChar() {
  // 匹配名字字符
  // NameChar ::= NameStartChar | "-" | "." | [0-9] | #xB7 |
  //              [#x0300-#x036F] | [#x203F-#x2040]
  if (!IS_LETTER(*curr) && !IS_NUMBER(*curr) && *curr != ':'
   && *curr != '_' && *curr != '-' && *curr != '.') {
    uint32_t uc;
    if (DecodeUTF8(&uc)) return true;
    if (!((uc == 0xB7)
          || (uc >= 0xD8 && uc <= 0xF6)
          || (uc >= 0xF8 && uc <= 0x2FF)
          || (uc >= 0x370 && uc <= 0x37D)
          || (uc >= 0x37F && uc <= 0x1FFF)
          || (uc >= 0x200C && uc <= 0x200D)
          || (uc >= 0x2070 && uc <= 0x218F)
          || (uc >= 0x2C00 && uc <= 0x2FEF)
          || (uc >= 0x3001 && uc <= 0xD7FF)
          || (uc >= 0xF900 && uc <= 0xFDCF)
          || (uc >= 0xFDF0 && uc <= 0xFFFD)
          || (uc >= 0x10000 && uc <= 0xEFFFF)
          || (uc >= 0x0300 && uc <= 0x036F)
          || (uc >= 0x203F && uc <= 0x2040))) {
      return true;
    }
  }

  return false;
}

bool XParser::MatchRefVal(ContentPtr& b, ContentPtr& e) {
  const char refChar = *curr;
  if (refChar != '\'' && refChar != '\"') {
    // TODO: 错误的符号，应该是 " 或者 '
    return true;
  }

  b = ++curr;

  while (*curr != refChar && curr != end)
    curr++;

  e = curr++;

  return IsEnd();
}

bool XParser::DecodeUTF8(uint32_t *unicode) {
  unsigned char *p = (unsigned char *) curr;
  if (*p < 0x7F) {
    *unicode = (uint32_t) *p;
    return false;
  }

  if ((p[0] & 0xE0) == 0xC0) {
    *unicode = (((uint32_t)p[0] & 0x7F) << 6) |
               (((uint32_t)p[1] & 0x3F));
    curr += 1;
  } else if ((p[0] & 0xF0) == 0xE0) {
    *unicode = (((uint32_t)p[0]  & 0x0F) << 12) |
               (((uint32_t)p[1]  & 0x3F) << 6)  |
               (((uint32_t)p[2]) & 0x3F);
    curr += 2;
  } else {
    *unicode = (((uint32_t)p[0] & 0x07) << 18) |
               (((uint32_t)p[1] & 0x3F) << 12) |
               (((uint32_t)p[2] & 0x3F) << 6)  |
               (((uint32_t)p[3] & 0x3F));
    curr += 3;
  }

  return IsEnd();
}


bool XParser::IsEnd() {
  // Maybe out the end pointer.
  if (curr >= end)
    //TODO: set error.
    return true;

  return false;
}

void XNode::setTxt(const char *t, int len) {
  if (len == -1)
    len = (int) strlen(t);
  txt.assign(t, len);
}

void XNode::setTxt(const std::string& t) {
  setTxt(t.c_str(), (int) t.length());
}

XNode *XNode::prev() {
  return (XNode *) llnode.prev;
}

XNode *XNode::next() {
  return (XNode *) llnode.next;
}

XElement::XElement()
: node(xNodeTypeElement)
, children(xNodeTypeNone)
{
  rbtree_init(&attrs, [](rbnode_t *a, rbnode_t *b) {
    XAttribute *aa = (XAttribute *) a;
    XAttribute *ab = (XAttribute *) b;
    return strcmp(aa->key.c_str(), ab->key.c_str());
  });

  llist_init(&children.llnode);
}

void deleteRBNode(XAttribute *attr) {
  if (!attr) return;
  deleteRBNode((XAttribute *)RBT_LEFT(&attr->rbnode));
  deleteRBNode((XAttribute *)RBT_RIGHT(&attr->rbnode));
  delete attr;
}

XElement::~XElement() {
  deleteRBNode((XAttribute *) attrs.root);

  for (XNode *n = children.next(); n != &children;) {
    XNode *tmp = n;
    n = n->next();
    llist_remove(&tmp->llnode);
    if (tmp->type == xNodeTypeElement) {
      delete (XElement *) tmp;
    } else {
      delete tmp;
    }
  }
}

XAttribute *XElement::addAttr(const char *key, int len) {
  XAttribute *attr = new XAttribute();
  attr->key.assign(key, len);
  rbtree_insert(&attrs, &attr->rbnode);
  return attr;
}

XAttribute *XElement::addAttr(const std::string& key) {
  return addAttr(key.c_str(), (int) key.length());
}

XAttribute *XElement::operator [] (const char *key) const {
  return findAttr(key);
}

XAttribute *XElement::operator [] (const std::string& key) const {
  return findAttr(key.c_str());
}

XAttribute *XElement::findAttr(const char *key) const {
  XAttribute *rbn = (XAttribute *) attrs.root;
  while (rbn) {
    int cmp = strcmp(rbn->key.c_str(), key);
    if (cmp < 0)      rbn = (XAttribute *) RBT_LEFT(&rbn->rbnode);
    else if (cmp > 0) rbn = (XAttribute *) RBT_RIGHT(&rbn->rbnode);
    else return rbn;
  }
  return nullptr;
}
XAttribute *XElement::findAttr(const std::string& key) const {
  return findAttr(key.c_str());
}

XComments *XElement::addChildComment() {
  XComments *comment = new XNode(xNodeTypeComment);
  llist_add(&children.llnode, &comment->llnode);
  return comment;
}

XText *XElement::addChildText() {
  XText *text = new XNode(xNodeTypeText);
  llist_add(&children.llnode, &text->llnode);
  return text;
}

XElement *XElement::addChildElement() {
  XElement *ele = new XElement();
  llist_add(&children.llnode, &ele->node.llnode);
  return ele;
}

XElement *XElement::first() {
  if (LLIST_EMPTY(&children.llnode))
    return nullptr;

  XNode *tmp = children.next();
  while (tmp != &children) {
    if (tmp->type == xNodeTypeElement) {
      return (XElement *) tmp;
    }
    tmp = tmp->next();
  }
  return nullptr;
}

XElement *XElement::last() {
  if (LLIST_EMPTY(&children.llnode))
    return nullptr;

  XNode *tmp = children.prev();
  while (tmp != &children) {
    if (tmp->type == xNodeTypeElement) {
      return (XElement *) tmp;
    }
    tmp = tmp->prev();
  }
  return nullptr;
}

XElement *XElement::prev() {
  XNode *item = node.prev();
  while (item->type != xNodeTypeNone) { // xNodeTypeNone 是根节点
    if (item->type == xNodeTypeElement)
      return (XElement *) item;
    item = item->prev();
  }
  return nullptr;
}

XElement *XElement::next() {
  XNode *item = node.next();
  while (item->type != xNodeTypeNone) { // xNodeTypeNone 是根节点
    if (item->type == xNodeTypeElement)
      return (XElement *) item;
    item = item->next();
  }
  return nullptr;
}

XDocument::XDocument() {
  root_ = nullptr;
}

XDocument::XDocument(const std::string& path) {
  root_ = nullptr;
  load(path);
}

XDocument::~XDocument() {
  if (root_) {
    delete root_;
  }
}

bool XDocument::load(const std::string& path) {
  XParser parser;

  // load fcontent.
  FILE *fp = fopen(path.c_str(), "rb");
  if (fp == NULL) {
    //xdcerr(x, xde_badfile, "can't open the xmlfile");
    return false;
  }

  fseek(fp, 0, SEEK_END);
  fpos_t len;
  fgetpos(fp, &len);
  char *content = (char *) malloc(len);
  if (content == NULL) {
    //xdcerr(x, xde_memory, "no enough memory");
    fclose(fp);
    return false;
  }

  fseek(fp, 0, SEEK_SET);
  size_t rn = fread(content, 1, len, fp);
  if (rn != len) {
    //
    free(content);
    return false;
  }
  fclose(fp);

  parser.curr = content;
  parser.end = content + len;
  parser.loadComments = false;
  parser.doc = this;
  bool res = !parser.parse();

  free(content);
  return res;
}

bool XDocument::save(const std::string& path) {
  return true;
}

XError XDocument::error() {
  return error_;
}

std::string XDocument::errorText() {
  return errtxt_;
}

XElement *XDocument::root() {
  return root_;
}

void XDocument::setRoot(XElement&& root) {
  if (root_ == nullptr)
    root_ = new XElement();

  root_->node.txt = std::move(root.node.txt);

  llist_move(&root_->children.llnode, &root.children.llnode);
  rbtree_move(&root_->attrs, &root.attrs);
}