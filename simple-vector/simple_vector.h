#pragma once

#include <cassert>
#include <initializer_list>
#include <algorithm>
#include <string>
#include <stdexcept>
#include <iostream>
#include <iterator>
#include <utility>

#include "array_ptr.h"

class ReserveProxyObj {
public:
    ReserveProxyObj(size_t capacity_to_reserve) : capacity_to_reserve_(capacity_to_reserve) {
    }

    size_t GetSizeReverse() {
        return capacity_to_reserve_;
    }
private:
    size_t capacity_to_reserve_;
};

template <typename Type>
class SimpleVector {
public:
    friend class ReserveProxyObj;

    using Iterator = Type*;
    using ConstIterator = const Type*;

    SimpleVector() noexcept = default;

    // Создаёт вектор из size элементов, инициализированных значением по умолчанию
    explicit SimpleVector(size_t size) {
        Reserve(size);
        size_ = size;
        capacity_ = size;
    }

    // Создаёт вектор из size элементов, инициализированных значением value
    SimpleVector(size_t size, const Type& value) {
        Reserve(size);
        size_ = size;
        capacity_ = size;
        std::generate(data_.Get(), data_.Get() + size, [&value]() { return value; });
    }

    // Создаёт вектор из std::initializer_list
    SimpleVector(std::initializer_list<Type> init) : data_(init.size()) {
        size_ = init.size();
        capacity_ = size_;
        std::move(init.begin(), init.end(), Iterator(data_.Get()));
    }

    SimpleVector(ReserveProxyObj other) {
        Reserve(other.GetSizeReverse());
        size_ = 0;
    }

    // Конструктор копирования
    SimpleVector(const SimpleVector& other) {
        // Так как переданный в конструктор обьект нельзя изменять, поэтому копируем поля класса в обьект-копию
        ArrayPtr<Type> copy(other.GetCapacity());
        try {
            std::copy(other.begin(), other.end(), copy.Get());

            data_.swap(copy);
            size_ = other.GetSize();
            capacity_ = other.GetCapacity();
            // вручную очищать память здесь нет необходимости, т.к. деструктор copy очистит память при выходе из функции
        }
        catch (...) {
            copy.~ArrayPtr(); // delete[] copy.Get();
            throw;
        }
    }

    // Конструктор перемещения
    SimpleVector(SimpleVector&& other) noexcept {
        *this = std::move(other);
    }

    // Копирование
    SimpleVector& operator=(const SimpleVector& rhs) {
        // Реализация оператора с использованием гарантии безопасности исключений
        // здесь не требуется, так ничего не может вызвать обрабатываемых исключений
        // А конструктор копирования сам обеспечиваем безопасность      
        if(this != &rhs) {
            SimpleVector copy(rhs);
            swap(copy);
        }
        return *this;
    }
    
    // Перемещение
    SimpleVector& operator=(SimpleVector&& rhs) noexcept {
        if (this != &rhs) {
            swap(rhs);
        }
        return *this;
    }

    // Добавляет элемент в конец вектора
    // При нехватке места увеличивает вдвое вместимость вектора

    // Копирование
    void PushBack(const Type& item) {
        if (size_ < capacity_) {
            data_[size_++] = item;          
        }
        else {
            Reserve(size_ + 1);
            data_[size_++] = item;           
        }
    }
    
    // Перемещение
    void PushBack(Type&& item) {
        if (size_ < capacity_) {
            data_[size_++] = std::move(item);
        }
        else {
            Reserve(size_*2 + 1);
            data_[size_++] = std::move(item);
        }
    }

    // Вставляет значение value в позицию pos.
    // Возвращает итератор на вставленное значение
    // Если перед вставкой значения вектор был заполнен полностью,
    // вместимость вектора должна увеличиться вдвое, а для вектора вместимостью 0 стать равной 1

    // Копирование
    Iterator Insert(ConstIterator pos, const Type& value) {
        assert(std::distance(cbegin(), pos) <= std::distance(cbegin(), cend()));
        
        // Совершаем меньше действий при вставке в конец
        if (pos == cend()) {
            PushBack(value);
            return std::prev(end());
        }

        if (size_ < capacity_) {
            return SeсureInsert(pos, value);
        }
        else {
            // Так как при недостатке капасити мы увеличиваем его, 
            // путем копирования в новую память, мы инвалидируем итератор
            // поэтому необходимо сохранить позицию вставляемого элемента
            size_t valid_pos = std::distance(cbegin(), pos);
            Resize(size_ + 1);
            // Вызываем функцию с валидным указателем
            return SeсureInsert((data_.Get() + valid_pos), value);
        }
    }

    // Перемещение
    Iterator Insert(ConstIterator pos, Type&& value) {
        assert(std::distance(cbegin(), pos) <= std::distance(cbegin(), cend()));

        // Совершаем меньше действий при вставке в конец
        if (pos == cend()) {
            PushBack(std::move(value));
            return std::prev(end());
        }

        if (size_ < capacity_) {
            return SeсureInsert(pos, std::move(value));
        }
        else {
            // Так как при недостатке капасити мы увеличиваем его путем копирования в новую память
            // тем самым инвалидируем итератор, поэтому необходимо сохранить позицию вставляемого элемента
            size_t valid_pos = std::distance(cbegin(), pos);
            Reserve(size_ + 1);
            // Вызываем функцию с валидным указателем
            return SeсureInsert(Iterator(data_.Get() + valid_pos), std::move(value));
        }
    }

    // Обеспечиваем вставку с гарантией безопасности искючений
    // Копирование
    Iterator SeсureInsert(ConstIterator pos, const Type& value) { 
        assert(std::distance(cbegin(), pos) <= std::distance(cbegin(), cend()));

        ArrayPtr<Type> copy(capacity_);
        try {
            size_t insert_pos = std::distance(cbegin(), pos);

            std::copy_backward(Iterator(data_.Get() + insert_pos), end(), std::next(end()));
            data_[insert_pos] = value;

            data_.swap(copy);
            ++size_;
            // Возвращаем указатель на вставленный элемент
            return Iterator(data_.Get() + insert_pos);
        }
        catch (...) {
            copy.~ArrayPtr();
            throw;
        }
    }

    // Перемещение
    Iterator SeсureInsert(ConstIterator pos, Type&& value) {
        assert(std::distance(cbegin(), pos) <= std::distance(cbegin(), cend()));

        size_t insert_pos = std::distance(cbegin(), pos);            
        std::move_backward(Iterator(data_.Get() + insert_pos), end(), std::next(end()));
        data_[insert_pos] = std::move(value);

        ++size_;
        return Iterator(data_.Get() + insert_pos);       
    }

    // "Удаляет" последний элемент вектора. Вектор не должен быть пустым
    void PopBack() noexcept {
        assert(size_);
        --size_;
    }

    // Удаляет элемент вектора в указанной позиции
    Iterator Erase(ConstIterator pos) {
        assert(std::distance(cbegin(), pos) < std::distance(cbegin(), cend()));

        ArrayPtr<Type> copy(size_);
        try {
            size_t erase_pos = std::distance(cbegin(), pos);

            Iterator end_ = std::move(std::make_move_iterator(begin()),
                std::make_move_iterator(begin() + erase_pos), Iterator(copy.Get()));
            std::move(std::make_move_iterator(std::next(begin() + erase_pos)),
                std::make_move_iterator(end()), end_);

            data_.swap(copy);
            --size_;
            return Iterator(data_.Get() + erase_pos);
        }
        catch (...) {
            copy.~ArrayPtr();
            throw;
        }
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= capacity_) {
            return;
        }
        ArrayPtr<Type> copy(new_capacity);
        try {
            // Перемещаем элементы диапазона в новый вектор с большей вместимостью
            std::move(begin(), end(), Iterator(copy.Get()));
            data_.swap(copy);        
            capacity_ = new_capacity;
        }
        catch (...) {
            copy.~ArrayPtr();
            throw;
        }
    }

    // Обменивает значение с другим вектором
    void swap(SimpleVector& other) noexcept {
        if (this != &other) {
            data_.swap(other.data_);           
            std::swap(size_, other.size_);
            std::swap(capacity_, other.capacity_);
        }
    }

    // Изменяет размер массива.
    // При увеличении размера новые элементы получают значение по умолчанию для типа Type
    void Resize(size_t new_size) {
        if ((new_size < size_)) {
            size_ = new_size;
            return;
        }
        ArrayPtr<Type> copy(new_size);
        try {
            std::move(std::make_move_iterator(begin()), std::make_move_iterator(end()), Iterator(copy.Get()));
            data_.swap(copy);

            size_ = new_size;
            capacity_ = new_size;
        }
        catch (...) {
            copy.~ArrayPtr();
            throw;
        }
    }

    // Возвращает константную ссылку на элемент с индексом index
    // Выбрасывает исключение std::out_of_range, если index >= size
    Type& At(size_t index) {
        if (index >= size_) {
            using namespace std::string_literals;
            throw std::out_of_range("Выход за пределы массива"s);
        }
        return data_[index];
    }

    // Возвращает константную ссылку на элемент с индексом index
    // Выбрасывает исключение std::out_of_range, если index >= size
    const Type& At(size_t index) const {
        if (index >= size_) {
            using namespace std::string_literals;
            throw std::out_of_range("Выход за пределы массива"s);
        }
        return data_[index];
    }

    // Возвращает количество элементов в массиве
    size_t GetSize() const noexcept {
        return size_;
    }

    // Возвращает вместимость массива
    size_t GetCapacity() const noexcept {
        return capacity_;
    }

    // Сообщает, пустой ли массив
    bool IsEmpty() const noexcept {
        return !size_;
    }

    // Возвращает ссылку на элемент с индексом index
    Type& operator[](size_t index) noexcept {
        return data_[index];
    }

    // Возвращает константную ссылку на элемент с индексом index
    const Type& operator[](size_t index) const noexcept {
        return data_[index];
    }

    // Обнуляет размер массива, не изменяя его вместимость
    void Clear() noexcept {
        size_ = 0;
    }

    // Возвращает итератор на начало массива
    // Для пустого массива может быть равен (или не равен) nullptr
    Iterator begin() noexcept {
        return Iterator(data_.Get());
    }

    // Возвращает итератор на элемент, следующий за последним
    // Для пустого массива может быть равен (или не равен) nullptr
    Iterator end() noexcept {
        return &data_[size_];
    }

    // Возвращает константный итератор на начало массива
    // Для пустого массива может быть равен (или не равен) nullptr
    ConstIterator begin() const noexcept {
        return Iterator(data_.Get());
    }

    // Возвращает итератор на элемент, следующий за последним
    // Для пустого массива может быть равен (или не равен) nullptr
    ConstIterator end() const noexcept {
        return &data_[size_];
    }

    // Возвращает константный итератор на начало массива
    // Для пустого массива может быть равен (или не равен) nullptr
    ConstIterator cbegin() const noexcept {
        return Iterator(data_.Get());
    }

    // Возвращает итератор на элемент, следующий за последним
    // Для пустого массива может быть равен (или не равен) nullptr
    ConstIterator cend() const noexcept {
        return &data_[size_];
    }
private:
    ArrayPtr<Type> data_;
    size_t size_ = 0;
    size_t capacity_ = 0;
};

ReserveProxyObj Reserve(size_t capacity_to_reserve) {
    return ReserveProxyObj(capacity_to_reserve);
}

template <typename Type>
inline bool operator==(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return !(lhs < rhs) && !(rhs < lhs);
}

template <typename Type>
inline bool operator!=(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return !(lhs == rhs);
}

template <typename Type>
inline bool operator<(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template <typename Type>
inline bool operator<=(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return (lhs < rhs) || (rhs == lhs);
}

template <typename Type>
inline bool operator>(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return std::lexicographical_compare(rhs.cbegin(), rhs.cend(), lhs.cbegin(), lhs.cend());
}

template <typename Type>
inline bool operator>=(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return !(lhs < rhs) || (rhs == lhs);
}