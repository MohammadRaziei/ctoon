#include "ctoon.h"
#include <iostream>

int main() {
    std::cout << "Ctoon C++ Library - Practical Examples" << std::endl;
    std::cout << "======================================" << std::endl << std::endl;

    // Example 1: Simple object with primitives
    std::cout << "Example 1: Simple Object" << std::endl;
    std::cout << "------------------------" << std::endl;
    {
        ctoon::Object user;
        user["id"] = std::make_shared<ctoon::Value>(123.0);
        user["name"] = std::make_shared<ctoon::Value>("Alice");
        user["active"] = std::make_shared<ctoon::Value>(true);
        user["score"] = std::make_shared<ctoon::Value>(95.5);
        
        std::cout << ctoon::dumpsJson(ctoon::Value(user), 4) << std::endl;
    }
    std::cout << std::endl;


    // Example 2: Nested objects
    std::cout << "Example 2: Nested Objects" << std::endl;
    std::cout << "-------------------------" << std::endl;
    {
          ctoon::Object address;
          address["street"] = std::make_shared<ctoon::Value>("123 Main St");
          address["city"] = std::make_shared<ctoon::Value>("Tehran");

          ctoon::Object person;
          person["name"] = std::make_shared<ctoon::Value>("Mohammad");
          person["age"] = std::make_shared<ctoon::Value>(30.0);
          person["address"] = std::make_shared<ctoon::Value>(address);

          std::cout << ctoon::dumpsJson(ctoon::Value(person), 1) << std::endl;
    }
    std::cout << std::endl;

      // Example 3: Primitive array
      std::cout << "Example 3: Primitive Array" << std::endl;
      std::cout << "--------------------------" << std::endl;
      {
          ctoon::Array tags;
          tags.push_back(std::make_shared<ctoon::Value>("programming"));
          tags.push_back(std::make_shared<ctoon::Value>("c++"));
          tags.push_back(std::make_shared<ctoon::Value>("serialization"));
        
          std::cout << ctoon::dumpsJson(ctoon::Value(tags)) << std::endl;
      }
      std::cout << std::endl;

      // Example 4: Tabular data (array of objects with same structure)
      std::cout << "Example 4: Tabular Data" << std::endl;
      std::cout << "-----------------------" << std::endl;
      {
          ctoon::Array users;
        
          ctoon::Object user1;
          user1["id"] = std::make_shared<ctoon::Value>(1.0);
          user1["name"] = std::make_shared<ctoon::Value>("Ali");
          user1["age"] = std::make_shared<ctoon::Value>(25.0);
          users.push_back(std::make_shared<ctoon::Value>(user1));
        
          ctoon::Object user2;
          user2["id"] = std::make_shared<ctoon::Value>(2.0);
          user2["name"] = std::make_shared<ctoon::Value>("Sara");
          user2["age"] = std::make_shared<ctoon::Value>(28.0);
          users.push_back(std::make_shared<ctoon::Value>(user2));
        
          ctoon::Object user3;
          user3["id"] = std::make_shared<ctoon::Value>(3.0);
          user3["name"] = std::make_shared<ctoon::Value>("Reza");
          user3["age"] = std::make_shared<ctoon::Value>(32.0);
          users.push_back(std::make_shared<ctoon::Value>(user3));
        
          ctoon::Object data;
          data["users"] = std::make_shared<ctoon::Value>(users);
        
          std::cout << ctoon::dumpsJson(ctoon::Value(data), 2) << std::endl;
      }
      std::cout << std::endl;

      // Example 5: Mixed data with different delimiters
      std::cout << "Example 5: Tab Delimited Data" << std::endl;
      std::cout << "-----------------------------" << std::endl;
      {
          ctoon::Array products;
        
          ctoon::Object product1;
          product1["sku"] = std::make_shared<ctoon::Value>("P-001");
          product1["name"] = std::make_shared<ctoon::Value>("Laptop");
          product1["price"] = std::make_shared<ctoon::Value>(1200);
          products.push_back(std::make_shared<ctoon::Value>(product1));
        
          ctoon::Object product2;
          product2["sku"] = std::make_shared<ctoon::Value>("P-002");
          product2["name"] = std::make_shared<ctoon::Value>("Mouse");
          product2["price"] = std::make_shared<ctoon::Value>(25.5);
          products.push_back(std::make_shared<ctoon::Value>(product2));
        
          ctoon::Object catalog;
          catalog["products"] = std::make_shared<ctoon::Value>(products);
        
//          ctoon::EncodeOptions options;
//          options.delimiter = ctoon::Delimiter::Tab;
          std::cout << ctoon::dumpsJson(ctoon::Value(catalog), 2) << std::endl;
      }
      std::cout << std::endl;

    // // Example 6: Complex nested structure
    // std::cout << "Example 6: Complex Nested Structure" << std::endl;
    // std::cout << "-----------------------------------" << std::endl;
    // {
    //     ctoon::Object order;
    //     order["orderId"] = std::make_shared<ctoon::Value>("ORD-12345");
    //     order["status"] = std::make_shared<ctoon::Value>("completed");

    //     ctoon::Array items;

    //     ctoon::Object item1;
    //     item1["product"] = std::make_shared<ctoon::Value>("Book");
    //     item1["quantity"] = std::make_shared<ctoon::Value>(2.0);
    //     item1["price"] = std::make_shared<ctoon::Value>(15.0);
    //     items.push_back(std::make_shared<ctoon::Value>(item1));
    //     
    //     ctoon::Object item2;
    //     item2["product"] = std::make_shared<ctoon::Value>("Pen");
    //     item2["quantity"] = std::make_shared<ctoon::Value>(5.0);
    //     item2["price"] = std::make_shared<ctoon::Value>(2.5);
    //     items.push_back(std::make_shared<ctoon::Value>(item2));

    //     order["items"] = std::make_shared<ctoon::Value>(items);

    //     ctoon::Object customer;
    //     customer["name"] = std::make_shared<ctoon::Value>("John Doe");
    //     customer["email"] = std::make_shared<ctoon::Value>("john@example.com");
    //     order["customer"] = std::make_shared<ctoon::Value>(customer);

    //     std::cout << ctoon::dumpsToon(ctoon::Value(order)) << std::endl;
    // }
    // std::cout << std::endl;

    // std::cout << "All examples completed successfully!" << std::endl;
    return 0;
}
