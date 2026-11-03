// Snyk Code C++ false positives. Each case is correct (checked at run time), yet Snyk Code flags it:
//   1 Path Traversal   2 Missing Release of Memory   3 Double Free
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>

int live = 0;
struct Parent;
struct Child {                                   // registers itself with its parent, which owns it
    Parent *parent; Child *next;
    explicit Child(Parent *p);
    ~Child();
};
struct Parent {
    Child *children = nullptr;
    ~Parent() { while (children) delete children; }
};
Child::Child(Parent *p) : parent(p), next(p->children) { p->children = this; ++live; }
Child::~Child() {
    for (Child **pp = &parent->children; *pp; pp = &(*pp)->next)
        if (*pp == this) { *pp = next; break; }  // unlink from the parent
    --live;
}

std::string StripSpaces(const char *path) {      // 1: "Path Traversal" (std::remove used as a path)
    std::ifstream f(path); std::string s; std::getline(f, s);
    s.erase(std::remove(s.begin(), s.end(), ' '), s.end());   // <algorithm> erase-remove: no file is touched
    return s;
}
void AddChild(Parent *p) { new Child(p); }       // 2: "never freed", but ~Parent frees it
Child *First(Parent *p) { return p->children; }
Child *ReplaceFirst(Parent *p) {                 // 3: "Double Free", but ~Child unlinks the old child
    delete First(p);
    new Child(p);
    return First(p);
}

int main() {
    std::ofstream("fp1.txt") << "a b c\n";
    std::string s = StripSpaces("fp1.txt");
    assert(s == "abc" && std::ifstream("fp1.txt").good());          // spaces gone, file still there
    { Parent p; AddChild(&p); AddChild(&p); assert(live == 2); }
    assert(live == 0);                                               // no leak
    { Parent p; AddChild(&p); Child *c = ReplaceFirst(&p); assert(live == 1 && p.children == c); }
    assert(live == 0);                                               // one delete each, no double free
    std::puts("PASSED: all 3 cases are correct");
}
