#ifndef __CACHE_HH__
#define __CACHE_HH__
#include<iostream>
#include<stdlib.h>
#include<errno.h>
using namespace std;
class DataNode{
public:
  char* block_path;// == "a/dog.txt/2"
  DataNode* prev;
  DataNode* next;
  DataNode(int x):block_index(x), block_path(NULL), prev(NULL), next(NULL) {}
  int index()const{return block_index;}
private:
  int block_index;//block_index should not change after this node created
};


class DLList{
public:
  DLList(): size(0){
    head = new DataNode(-1);
    tail = new DataNode(-1);
    head->next = tail;
    tail->prev = head;
    head->prev = NULL;
    tail->next = NULL;
  }
  DataNode* getHead()const;
  DataNode* getTail()const;
  void addToTail(DataNode* node);
  void addToHead(DataNode* node);
  void refresh(DataNode* node);
  void remove(DataNode* node);
  void print()const;
  int getSize() const;
  ~DLList();
private:
  int size;
  DataNode* head;
  DataNode* tail;
  
};

int DLList::getSize()const{
  return size;
}
DLList::~DLList(){
  delete head;
  delete tail;
}
DataNode*  DLList::getHead() const{
  return head->next;
}
DataNode* DLList::getTail() const{
  return tail->prev;
}

void DLList::addToTail(DataNode* node){
  tail->prev->next = node;
  node->prev = tail->prev;
  node->next = tail;
  tail->prev = node;
  size++;
}

void DLList::remove(DataNode* node) {
  node->prev->next = node->next;
  node->next->prev = node->prev;
  node->prev = NULL;
  node->next = NULL;
  size--;
}
void DLList::addToHead(DataNode* node) {
    node->next = head->next;
    head->next->prev = node;
    head->next = node;
    node->prev = head;
    size++;
}

void DLList::refresh(DataNode* node){
  remove(node);
  addToHead(node);
}

void DLList::print()const{
  DataNode* tmp = head;
  while(tmp != NULL) {
    cout << tmp->index() << " -> ";
    tmp = tmp->next;
  }
  cout << endl;
}

#endif
