// Решите загадку: Сколько чисел от 1 до 1000 содержат как минимум одну цифру 3?
// Напишите ответ здесь: 300
#include <string>
#include <iostream>

using namespace std;

int main() {
    char s[4] = "";
    int answer = 0;
    for (int i = 0; i < 1000; ++i) {
        itoa(i, s, 10);
        for (char c : s) {
            if (c == '3'){
                ++answer;
            }
        }
    }
    cout << answer;
}

// Закомитьте изменения и отправьте их в свой репозиторий.

// Проверка работы CLion