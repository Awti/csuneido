#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "hashtbl.h"

// hash map template, similar to STL, uses Hashtbl
template <class Key, class Val, class Hfn = HashFn<Key>,
	class Keq = KeyEq<Key> >
class HashMap {
private:
	// key + value pair
	struct Slot {
		explicit Slot(const Key& k) : key(k) {
		}
		Slot(const Key& k, const Val& v) : key(k), val(v) {
		}
		bool operator==(const Slot& slot) const {
			return Keq()(key, slot.key) && val == slot.val;
		}
		Key key;
		Val val;
	};
	// key extraction function object for Hashtbl
	struct kofv {
		Key operator()(const Slot& slot) {
			return slot.key;
		}
	};
	typedef Hashtbl<Key, Slot, kofv, Hfn, Keq> Htbl;
	Htbl tbl;

public:
	explicit HashMap(size_t n = 0) : tbl(n) {
	}
	Val& operator[](const Key& key) {
		return tbl.insert(Slot(key)).val;
	}
	const Val& operator[](const Key& key) const {
		Slot* p = tbl.find(key);
		verify(p);
		return p->val;
	}
	Val* find(const Key& key) const {
		Slot* p = tbl.find(key);
		return p ? &p->val : NULL;
	}
	bool erase(const Key& key) {
		return tbl.erase(key);
	}
	void clear() {
		tbl.clear();
	}
	size_t size() const {
		return tbl.size();
	}
	bool empty() const {
		return tbl.empty();
	}
	bool operator==(const HashMap& h) const {
		return tbl == h.tbl;
	}

	class iterator {
	public:
		iterator() : iter() {
		}
		explicit iterator(const typename Htbl::iterator& _iter) : iter(_iter) {
		}
		Slot& operator*() {
			return *iter;
		}
		Slot* operator->() {
			return &(*iter);
		}
		iterator& operator++() {
			++iter;
			return *this;
		}
		bool operator==(const iterator& _iter) const {
			return iter == _iter.iter;
		}

	private:
		typename Htbl::iterator iter;
	};
	iterator begin() {
		return iterator(tbl.begin());
	}
	iterator end() {
		return iterator(tbl.end());
	}

	class const_iterator {
	public:
		explicit const_iterator(const typename Htbl::const_iterator& _iter)
			: iter(_iter) {
		}
		Slot& operator*() {
			return *iter;
		}
		Slot* operator->() {
			return &(*iter);
		}
		const_iterator& operator++() {
			++iter;
			return *this;
		}
		bool operator==(const const_iterator& _iter) const {
			return iter == _iter.iter;
		}

	private:
		typename Htbl::const_iterator iter;
	};

	const_iterator begin() const {
		return const_iterator(tbl.begin());
	}
	const_iterator end() const {
		return const_iterator(tbl.end());
	}
};

// output a HashMap to a stream
template <class K, class T, class H, class E>
Ostream& operator<<(Ostream& os, const HashMap<K, T, H, E>& map) {
	os << "{";
	for (auto i = map.begin(); i != map.end(); ++i) {
		if (i != map.begin())
			os << ", ";
		os << i->key << "=" << i->val;
	}
	os << "}";
	return os;
}
