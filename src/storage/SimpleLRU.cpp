#include <iostream>
#include "SimpleLRU.h"

namespace Afina {
    namespace Backend {

// See MapBasedGlobalLockImpl.h
        bool SimpleLRU::Put(const std::string &key, const std::string &value) {
            if (_lru_index.find(std::ref(key)) != _lru_index.end())
                SimpleLRU::Delete(key);

            int free_space = _max_size - _cur_size;
            int need_space = key.size() + value.size();

            if (need_space > _max_size)
                return false;

            if (need_space > free_space)
                _free_memory(need_space - free_space);

            lru_node *node = new lru_node;
            _create_node(key, value, std::unique_ptr<lru_node >(node));
            _lru_index.insert(std::pair<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node> > (node->key, *node) );
            return true;
        }

// See MapBasedGlobalLockImpl.h
        bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {
            if(_lru_index.find(std::ref(key)) == _lru_index.end())
                return SimpleLRU::Put(key, value);
            return false;
        }

// See MapBasedGlobalLockImpl.h
        bool SimpleLRU::Set(const std::string &key, const std::string &value) {
            if (_lru_index.find(std::ref(key)) == _lru_index.end())
                return false;
            SimpleLRU::Delete(key);
            return SimpleLRU::Put(key, value);
        }

// See MapBasedGlobalLockImpl.h
        bool SimpleLRU::Delete(const std::string &key) {
            auto iter = _lru_index.find(std::ref(key));
            if (iter != _lru_index.end()) {
                lru_node *node = &(iter->second.get());
                _lru_index.erase(std::ref(key));
                _remove_node(node);
                return true;
            }
            return false;
        }

// See MapBasedGlobalLockImpl.h
        bool SimpleLRU::Get(const std::string &key, std::string &value) {
            auto iter = _lru_index.find(std::ref(key));
            if (iter != _lru_index.end()) {
                lru_node *node = &(iter->second.get());
                value = iter->second.get().value;
                // save value to not destroy it int _remove_node()
                std::unique_ptr<lru_node> ptr = std::move(node->prev->next);
                _remove_node(node);
                _create_node(node->key, node->value, std::move(ptr));
                return true;
            }
            return false;
        }


        bool SimpleLRU::_create_node(const std::string &key, const std::string &value, std::unique_ptr<lru_node> ptr_node)
        {
            lru_node *node = ptr_node.get();
            node->key = key;
            node->value = value;
            _cur_size += key.size() + value.size();

            // Let's add element to the end of list. List is cycle;
            // First case: no elements in list, make it cycle to itself;
            if (!_lru_head) {
                _lru_head = node;
                _lru_head->next = std::move(ptr_node);
                _lru_head->prev = _lru_head;
                // Second case: one element in list;
            } else if (_lru_head->prev == _lru_head) {
                node->next = std::move(_lru_head->next);
                _lru_head->next = std::move(ptr_node);
                _lru_head->prev = node;
                node->prev = _lru_head;
                // Third case: >1 elements in list;
            } else {
                node->next = std::move(_lru_head->prev->next);
                node->prev = _lru_head->prev;
                _lru_head->prev->next = std::move(ptr_node);
                _lru_head->prev = node;
            }
            return true;
        }

        bool SimpleLRU::_remove_node(lru_node *node) {
            _cur_size -= node->key.size() + node->value.size();
            if (node->prev == node){
                node->next.reset();
                _lru_head = nullptr;
            } else {
                if (node == _lru_head) {
                    _lru_head = node->next.get();
                }
                node->next->prev = node->prev;
                node->prev->next = std::move(node->next);
            }
            return true;
        }

        bool SimpleLRU::_free_memory(int need_size) {
            while (need_size > 0) {
                size_t freed_space = _lru_head->key.size() + _lru_head->value.size();
                need_size -= freed_space;
                _lru_index.erase(_lru_head->key);
                _remove_node(_lru_head);
            }
            return true;

        }

    } // namespace Backend
} // namespace Afina



