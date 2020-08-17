/*
 * Copyright (c) 2018, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */

#ifndef QUERYBLAZER_PREFIXTREE_H
#define QUERYBLAZER_PREFIXTREE_H

#include "common.h"
#include <map>
#include <queue>
#include <vector>

namespace qbz {

template <typename Key, typename Value>
class PrefixNode;

template <typename Key, typename Value>
class PrefixTree;

template<typename Key>
class Trie {
  public:
    struct Data {
        explicit Data(size_t count = 0) : count{count} {}
        size_t count;
    };

    void Insert(const std::vector<Key> &query, size_t count) {
        auto node = root.Find(query, true);
        if (node->data) node->data->count += count;
        else node->data = new Data{count};
    }

    const PrefixNode<Key, Data> &Root() const { return root; }

  private:
    PrefixNode<Key, Data> root;
};

/**
 * wrapper around node pointer
 */
template <typename Key, typename Value>
class PrefixLeaf {
  public:
    // no copies allowed
    PrefixLeaf(const PrefixLeaf &) = delete;

    PrefixLeaf &operator=(const PrefixLeaf &) = delete;

    // move constructor allowed
    // any subsequent method call will result in segfault
    PrefixLeaf(PrefixLeaf &&that) noexcept : node{that.node} {
        that.node = nullptr;
    }

    // move operator is allowed
    PrefixLeaf &operator=(PrefixLeaf &&) noexcept = default;

    std::vector<Key> Prefix() const { return node->Prefix(); }

    const Value &Data() const { return *node->Data(); }

    Value &Data() { return *node->Data(); }

    size_t Size() const { return node->Size(); }

    size_t Depth() const { return node->Depth(); }

  private:
    explicit PrefixLeaf(PrefixNode<Key, Value> &node) : node{&node} {}

    PrefixNode<Key, Value> *node;

    friend class PrefixTree<Key, Value>;

    friend class PrefixNode<Key, Value>;
};

/**
 * Actual implementation of PrefixTree
 * @tparam Key
 * @tparam Value
 */
template <typename Key, typename Value>
class PrefixNode {
  public:
    ~PrefixNode() {
        delete data;
        for (auto &child : children) { delete child.second; }
    }

    std::vector<Key> Prefix() const {
        std::vector<Key> prefix;
        auto node = this;
        while (!node->IsRoot()) {
            prefix.push_back(node->key);
            node = node->parent;
        }
        std::reverse(prefix.begin(), prefix.end());
        return prefix;
    }

    /**
     * Return all leafs
     */
    std::vector<PrefixLeaf<Key, Value>> FindAll(const std::vector<Key> &keys) {
        if (Empty()) return {};
        auto node = Find(keys);
        if (!node) return {};

        std::vector<PrefixLeaf<Key, Value>> leafs;
        std::queue<PrefixNode *> queue;
        queue.push(node);
        while (!queue.empty()) {
            node = queue.front();
            queue.pop();

            if (node->data)
                leafs.push_back(std::move(PrefixLeaf<Key, Value>{*node}));
            for (auto &pair : node->children)
                if (!pair.second->Empty()) queue.push(pair.second);
        }

        return leafs;
    }

    bool Insert(const std::vector<Key> &keys, Value value) {
        auto node = Find(keys, true);
        if (node->data) return false;
        node->data = new Value{std::move(value)};
        while (node) {
            ++node->num_leafs;
            node = node->parent;
        }
        return true;
    }

    /**
     * Remove the current leaf
     */
    void Erase() {
        QBZ_ASSERT(data != nullptr, "Not a leaf");
        delete data;
        data = nullptr;
        auto node = this;
        while (node) {
            --node->num_leafs;
            node = node->parent;
        }
    }

    const Value *Data() const { return data; }

    Value *Data() { return data; }

    size_t Size() const { return num_leafs; }

    bool Empty() const { return Size() == 0; }

    size_t Depth() const { return depth; }

    const std::map<Key, PrefixNode *> &Children() const {
        return children;
    };

  private:
    // all constructors not allowed by client
    PrefixNode()
        : parent{nullptr},
          key{Key{}},
          data{nullptr},
          num_leafs{0},
          depth{0} {}

    explicit PrefixNode(PrefixNode *const parent, Key key)
        : parent{parent},
          key{std::move(key)},
          data{nullptr},
          num_leafs{0},
          depth{parent->depth + 1} {}

    explicit PrefixNode(PrefixNode *const parent, Key key, Value value)
        : parent{parent},
          key{std::move(key)},
          data{new Value{std::move(value)}},
          num_leafs{1},
          depth{parent->depth + 1} {}

    bool IsRoot() const { return parent == nullptr; }

    /**
     * Find the node relative to this by keys
     * If create, construct node as you go
     */
    PrefixNode *Find(const std::vector<Key> &keys, bool create = false) {
        auto node = this;
        for (auto &k : keys) {
            auto it = node->children.find(k);
            if (it == node->children.end()) {
                if (create)
                    it = node->children.emplace(k, new PrefixNode{node, k})
                             .first;
                else
                    return nullptr;
            }
            node = it->second;
        }
        return node;
    }

    PrefixNode *const parent;
    const Key key;
    std::map<Key, PrefixNode *> children;
    Value *data;
    size_t num_leafs;
    const size_t depth;

    friend class PrefixTree<Key, Value>;
    friend class Trie<Key>;
};

/**
 * Wrapper around a PrefixNode as a root
 * @tparam Key
 * @tparam Value
 */
template <typename Key, typename Value>
class PrefixTree {
  public:
    /**
     * Return all leafs
     */
    std::vector<PrefixLeaf<Key, Value>>
    FindAll(const std::vector<Key> &keys = {}) {
        return root.FindAll(keys);
    }

    /**
     * Insert given value at the given keys
     */
    bool Insert(const std::vector<Key> &keys, Value value) {
        return root.Insert(keys, std::move(value));
    }

    /**
     * Insert given value at the given keys relative to the leaf
     * @return
     */
    bool Insert(PrefixLeaf<Key, Value> &leaf, const std::vector<Key> &keys,
                Value value) {
        return leaf.node->Insert(keys, std::move(value));
    }

    /**
     * Clear all leafs
     */
    void Clear() {
        for (auto &child : root.children) delete child.second;
        root.children.clear();
        root.num_leafs = 0;
    }

    /**
     * Erase leaf & invalidate it
     * any further usage of leaf will result in seg fault
     */
    void Erase(PrefixLeaf<Key, Value> &leaf) {
        leaf.node->Erase();
        leaf.node = nullptr;
    }

    /**
     * number of leafs in the tree
     */
    size_t Size() const { return root.Size(); }

    bool Empty() const { return root.Empty(); }

  private:
    PrefixNode<Key, Value> root;
};

} // namespace qbz

#endif // QUERYBLAZER_PREFIXTREE_H