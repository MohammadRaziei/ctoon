#include "ctoon.h"
#include <iostream>

int main() {
    std::cout << "Tabular Data Example" << std::endl;
    std::cout << "====================" << std::endl;

    // Create employee data
    ctoon::Array employees;

    // Employee 1
    ctoon::Object emp1;
    emp1["id"] = std::make_shared<ctoon::Value>(101.0);
    emp1["name"] = std::make_shared<ctoon::Value>("Ali Rezaei");
    emp1["department"] = std::make_shared<ctoon::Value>("Engineering");
    emp1["salary"] = std::make_shared<ctoon::Value>(75000.0);
    emp1["active"] = std::make_shared<ctoon::Value>(true);
    employees.push_back(std::make_shared<ctoon::Value>(emp1));

    // Employee 2
    ctoon::Object emp2;
    emp2["id"] = std::make_shared<ctoon::Value>(102.0);
    emp2["name"] = std::make_shared<ctoon::Value>("Sara Mohammadi");
    emp2["department"] = std::make_shared<ctoon::Value>("Marketing");
    emp2["salary"] = std::make_shared<ctoon::Value>(65000.0);
    emp2["active"] = std::make_shared<ctoon::Value>(true);
    employees.push_back(std::make_shared<ctoon::Value>(emp2));

    // Employee 3 (inactive)
    ctoon::Object emp3;
    emp3["id"] = std::make_shared<ctoon::Value>(103.0);
    emp3["name"] = std::make_shared<ctoon::Value>("Reza Karimi");
    emp3["department"] = std::make_shared<ctoon::Value>("Sales");
    emp3["salary"] = std::make_shared<ctoon::Value>(70000.0);
    emp3["active"] = std::make_shared<ctoon::Value>(false);
    employees.push_back(std::make_shared<ctoon::Value>(emp3));

    ctoon::Object data;
    data["employees"] = std::make_shared<ctoon::Value>(employees);

    std::cout << "-------------------" << std::endl;
    std::cout << ctoon::dumpsToon(ctoon::Value(data)) << std::endl;

    return 0;
}
