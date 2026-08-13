//
// Created by Mathias Vatter on 13.08.26.
//

#pragma once

#include <mutex>
#include <string>
#include <unordered_set>

/// One shared copy of every source path the compiler reads.
///
/// A <Token> carries the path of the file it came from, and a compile keeps millions of them alive
/// at once - one per token of every file, plus one inside every AST node, plus a copy for every
/// clone that inlining and monomorphization make. Holding the path in the token itself meant a heap
/// allocation per token (a path is far too long for the small string buffer) and a string copy per
/// clone, all for the handful of distinct paths a program is built from. Tokens point into this
/// table instead, so copying one is copying a pointer.
///
/// Entries are never removed: the pointers handed out have to stay valid as long as any token does,
/// and there is one entry per file, not per token. <std::unordered_set> keeps its elements at stable
/// addresses when it rehashes, which is what makes those pointers safe - a vector would move them.
class FileTable {
public:
	/// The stored path equal to <path>, inserting it on first sight.
	static const std::string* intern(const std::string& path) {
		if (path.empty()) return &none();
		std::lock_guard lock(mutex());
		return &*paths().insert(path).first;
	}

	/// The path of a token that stands in no file, which is every token the compiler builds itself.
	static const std::string& none() {
		static const std::string empty;
		return empty;
	}

private:
	static std::unordered_set<std::string>& paths() {
		static std::unordered_set<std::string> paths;
		return paths;
	}

	static std::mutex& mutex() {
		static std::mutex mutex;
		return mutex;
	}
};
