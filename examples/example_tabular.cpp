#include "ctoon.h"
#include <iostream>

int main() {
    std::cout << "Tabular Data Example" << std::endl;
    std::cout << "====================" << std::endl;

    // Create employee data
    ctoon::Array employees;

    // Employee 1
    ctoon::Object emp1;
    emp1["id"] = ctoon::Value(101.0);
    emp1["name"] = ctoon::Value("Ali Rezaei");
    emp1["department"] = ctoon::Value("Engineering");
    emp1["salary"] = ctoon::Value(75000.0);
    emp1["active"] = ctoon::Value(true);
    employees.push_back(emp1);

    // Employee 2
    ctoon::Object emp2;
    emp2["id"] = ctoon::Value(102.0);
    emp2["name"] = ctoon::Value("Sara Mohammadi");
    emp2["department"] = ctoon::Value("Marketing");
    emp2["salary"] = ctoon::Value(65000.0);
    emp2["active"] = ctoon::Value(true);
    employees.push_back(emp2);

    // Employee 3 (inactive)
    ctoon::Object emp3;
    emp3["id"] = ctoon::Value(103.0);
    emp3["name"] = ctoon::Value("Reza Karimi");
    emp3["department"] = ctoon::Value("Sales");
    emp3["salary"] = ctoon::Value(70000.0);
    emp3["active"] = ctoon::Value(false);
    employees.push_back(emp3);

    ctoon::Object data;
    data["employees"] = ctoon::Value(employees);

    std::cout << "-------------------" << std::endl;
    std::cout << ctoon::dumpsToon(ctoon::Value(data)) << std::endl;

    return 0;
}
