# Small vector

В этой задаче вам предстоит реализовать аналог [boost::small_vector](https://www.boost.org/doc/libs/1_68_0/doc/html/boost/container/small_vector.html)

Пользоваться контейнерами из std - нельзя.

Ключевые особенности SmallVector:

- Изначально выделяет небольшой (задается шаблонным параметром) кусок памяти на стеке
- Если память на стеке кончается - переносит все на кучу и начинает выделять память на куче

Необходимо реализовать следующие методы

- Конструкторы
    - ```SmallVector()``` --- создает пустой small_vector (с выделенной на стэке памятью)
    - ```SmallVector(size_t size)```  --- создает small_vector из size объектов равных T()
    - ```SmallVector(size_t size, const T& obj)```  --- создает small_vector из size объектов равных obj
    - ```SmallVector(const SmallVector& other)```
- Операторы
    - ```operator=(const SmallVector&)```
    - ```operator[](size_t)```
- Методы
    - ```size()```
    - ```reserve(size_t n)``` --- резервирует память под n объектов
    - ```resize(size_t n)``` --- изменяет размер, если получилось больше - заполняет T()
    - ```push_back(const T&)```
    - ```pop_back()```
- Итераторы
    - ```Iterator begin()```
    - ```Iterator end()```
    - ```constIterator cbegin()```
    - ```constIterator cend()```

