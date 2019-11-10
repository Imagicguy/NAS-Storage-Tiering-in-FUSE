#ifndef __CACHE_HH__
#define __CACHE_HH__

#include<stdlib.h>
#include<errno.h>
using namespace std;
class DataNode{
public:
  string block_path;
  DataNode* prev;
  DataNode* next;
  DataNode(int x):block_index(x), block_path(NULL), prev(NULL), next(NULL) {}
private:
  int block_index;//block_index should not change after this node created
};

class DLList{
public:
  DLList(): size(0), head(NULL), tail(NULL){}
  DataNode* getHead();
  int addToTail();
  void refresh(DataNode* node);
private:
  int size;
  DataNode* head;
  DataNode* tail;
  
};

DataNode*  DLList::getHead() const{
  return head;
}

int DLList::addToTail(DataNode* node){
  if (tail != NULL) {
    tail->next = node;
  }
  tail = node;
}

void DLList::remove(DataNode* node) {
  node->prev->next = node->next;
  node->next->prev = node->prev;
  node->prev = NULL;
  node->next = NULL;
}
void DLList::addToHead(DataNode* node) {
  if (head == NULL) {
    head = node;
  }else {
    node->next = head->next;
    head->next->prev = node;
    head = node;
    
  }
}
void DLList::refresh(DataNode* node){
  remove(node);
  addToHead(node);
}


#endif
