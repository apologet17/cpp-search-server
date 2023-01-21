# cpp-search-server
It's a first complex project written on C++ during Yandex Praktikum course

# Description
The search server provides a complex search of documents based on query words, stop words, munis words and document status. The search algorithm is based on TF-IDF statistics with parallel execution support.

File I/O operations are not realized in this version, currently the documents are added to base inside main file. Several indexes are generated to increase document's search. During the search the app uses ConcurrentMap - a developed multi-thread wrap for std::map with an r/w support.

Also realized a class Paginator which helps to paginate search results in several pages.

During documents adding several dublicats may be included. Function RemoveDuplicates realizes a search and removing of dublicated items.

The search server is optimized for fast performance and low memory usage due to enhanced algorithms, string_view and move semantic usage.

# Used language features
OOP, templates, lyambda functions, std algorithms, parallel calculations, multi-thread operations, TDD, code profiling.

# Build
CMakeLists.txt file is included for fast build with CMAKE. Only STL library is used.
