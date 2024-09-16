#include <iostream>
#include <fstream>
#include <string>
#include <modbus/modbus.h>
#include <nlohmann/json.hpp> // nlohmann/json для работы с JSON

using namespace std;

#define TAG_COUNT 1000  // Количество регистров

int registers[TAG_COUNT] = {0};  // Хранение регистров ModBus

using json = nlohmann::json;

// Функция для загрузки конфигурации из файла
int load_config(const string& filename, string &mode, int &port, string &serial_port, int &baud_rate, char &parity, int &data_bits, int &stop_bits) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error opening config file." << endl;
        return -1;
    }

    json config;
    try {
        file >> config;  // Чтение файла в объект JSON
    } catch (json::parse_error& e) {
        cerr << "JSON parse error: " << e.what() << endl;
        return -1;
    }
    file.close();

    // Чтение параметров
    mode = config.at("mode").get<string>();                 // Режим (TCP или RTU)
    port = config.at("port").get<int>();                          // TCP порт
    serial_port = config.at("serial_port").get<string>();    // Последовательный порт для RTU
    baud_rate = config.at("baud_rate").get<int>();                // Скорость для RTU
    parity = config.at("parity").get<string>()[0];           // Четность (символ)
    data_bits = config.at("data_bits").get<int>();                // Количество бит данных
    stop_bits = config.at("stop_bits").get<int>();                // Количество стоп-битов

    // Чтение начальных значений регистров
    const json& json_registers = config.at("registers"); // создаётся постоянная ссылка на этот JSON-массив, чтобы избежать копирования данных
    for (size_t i = 0; i < json_registers.size() && i < TAG_COUNT; ++i) {
        registers[i] = json_registers[i].get<int>();
    }

    return 0;
}

// Функция для сохранения текущих регистров в файл конфигурации
void save_config(const string& filename) {
    json config;

    // Сохраняем текущее состояние регистров
    config["registers"] = json::array();
    for (int i = 0; i < TAG_COUNT; ++i) {
        config["registers"].push_back(registers[i]); // push_back добавляет каждое значение регистра в массив registers внутри объекта config.
    }

    // Сохраняем данные в файл
    ofstream file(filename);
    if (!file.is_open()) {
        cerr << "Error opening config file for saving." << endl;
        return;
    }
    file << config.dump(4);  // Сохраняем в файл с форматированием
    file.close();
}

int main() {
    string mode, serial_port;
    int port, baud_rate, data_bits, stop_bits;
    char parity;

    // Загрузка конфигурации
    if (load_config("config.json", mode, port, serial_port, baud_rate, parity, data_bits, stop_bits) == -1) {
        return -1;
    }

    modbus_t *ctx; // указатель на структуру modbus_t, используется для хранения контекста соединения с Modbus сервером
    
    if (mode == "TCP") {
        // Создаем контекст ModBus TCP
        ctx = modbus_new_tcp("127.0.0.1", port);
        if (ctx == NULL) {
            cerr << "Unable to create Modbus TCP context" << endl;
            return -1;
        }
    } else if (mode == "RTU") {
        // Создаем контекст ModBus RTU
        ctx = modbus_new_rtu(serial_port.c_str(), baud_rate, parity, data_bits, stop_bits); // принимает параметры для настройки последовательного порта
        if (ctx == NULL) {
            cerr << "Unable to create Modbus RTU context" << endl;
            return -1;
        }

        // Устанавливаем идентификатор устройства (Slave ID)
        modbus_set_slave(ctx, 1); // устанавливает идентификатор Slave ID равный 1 для подчиненного устройства в контексте Modbus RTU
    } else {
        cerr << "Unknown mode specified in config: " << mode << endl;
        return -1;
    }

    // Подключаемся
    cerr << "Attempting to connect to ModBus server" << endl;
    if (modbus_connect(ctx) == -1) {
        cerr << "Connection failed " << modbus_strerror(errno) << endl;
        modbus_free(ctx);
        return -1;
    }
    cerr << "Connected successfully" << endl;

    // Основной цикл обработки ModBus запросов
    for (;;) {
        uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH]; //Массив для хранения запроса
        int reception = modbus_receive(ctx, query); // Приём запроса от ведущего устройства. reception указывает на кол-во байтов в запросе
        // Обработка успешного запроса
        if (reception > 0) {
            modbus_reply(ctx, query, reception, NULL); // отправляет ответ на полученный запрос
            save_config("config.json");  // Сохраняем текущее состояние регистров
        } else if (reception == -1) {
            // Обрабатываем ошибку
            cerr << "Error during communication" << endl;
            break;
        }
    }

    // Освобождаем ресурсы
    modbus_free(ctx);
    return 0;
}