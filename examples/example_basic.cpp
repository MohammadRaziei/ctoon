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
        user["id"] = ctoon::Value(123.0);
        user["name"] = ctoon::Value("Alice");
        user["active"] = ctoon::Value(true);
        user["score"] = ctoon::Value(95.5);
        
        std::cout << ctoon::dumpsJson(ctoon::Value(user), 4) << std::endl;
    }
    std::cout << std::endl;


    // Example 2: Nested objects
    std::cout << "Example 2: Nested Objects" << std::endl;
    std::cout << "-------------------------" << std::endl;
    {
          ctoon::Object address;
          address["street"] = ctoon::Value("123 Main St");
          address["city"] = ctoon::Value("Tehran");

          ctoon::Object person;
          person["name"] = ctoon::Value("Mohammad");
          person["age"] = ctoon::Value(30.0);
          person["address"] = ctoon::Value(address);

          std::cout << ctoon::dumpsJson(ctoon::Value(person), 1) << std::endl;
    }
    std::cout << std::endl;

      // Example 3: Primitive array
      std::cout << "Example 3: Primitive Array" << std::endl;
      std::cout << "--------------------------" << std::endl;
      {
          ctoon::Array tags;
          tags.push_back(ctoon::Value("programming"));
          tags.push_back(ctoon::Value("c++"));
          tags.push_back(ctoon::Value("serialization"));
        
          std::cout << ctoon::dumpsJson(ctoon::Value(tags)) << std::endl;
      }
      std::cout << std::endl;

      // Example 4: Tabular data (array of objects with same structure)
      std::cout << "Example 4: Tabular Data" << std::endl;
      std::cout << "-----------------------" << std::endl;
      {
          ctoon::Array users;
        
          ctoon::Object user1;
          user1["id"] = ctoon::Value(1.0);
          user1["name"] = ctoon::Value("Ali");
          user1["age"] = ctoon::Value(25.0);
          users.push_back(user1);
        
          ctoon::Object user2;
          user2["id"] = ctoon::Value(2.0);
          user2["name"] = ctoon::Value("Sara");
          user2["age"] = ctoon::Value(28.0);
          users.push_back(user2);
        
          ctoon::Object user3;
          user3["id"] = ctoon::Value(3.0);
          user3["name"] = ctoon::Value("Reza");
          user3["age"] = ctoon::Value(32.0);
          users.push_back(user3);
        
          ctoon::Object data;
          data["users"] = ctoon::Value(users);
        
          std::cout << ctoon::dumpsJson(ctoon::Value(data), 2) << std::endl;
      }
      std::cout << std::endl;

      // Example 5: Mixed data with different delimiters
      std::cout << "Example 5: Tab Delimited Data" << std::endl;
      std::cout << "-----------------------------" << std::endl;
      {
          ctoon::Array products;
        
          ctoon::Object product1;
          product1["sku"] = ctoon::Value("P-001");
          product1["name"] = ctoon::Value("Laptop");
          product1["price"] = ctoon::Value(1200);
          products.push_back(product1);
        
          ctoon::Object product2;
          product2["sku"] = ctoon::Value("P-002");
          product2["name"] = ctoon::Value("Mouse");
          product2["price"] = ctoon::Value(25.5);
          products.push_back(product2);
        
          ctoon::Object catalog;
          catalog["products"] = ctoon::Value(products);
        
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
    //     order["orderId"] = ctoon::Value("ORD-12345");
    //     order["status"] = ctoon::Value("completed");
        
    //     ctoon::Array items;
        
    //     ctoon::Object item1;
    //     item1["product"] = ctoon::Value("Book");
    //     item1["quantity"] = ctoon::Value(2.0);
    //     item1["price"] = ctoon::Value(15.0);
    //     items.push_back(item1);
        
    //     ctoon::Object item2;
    //     item2["product"] = ctoon::Value("Pen");
    //     item2["quantity"] = ctoon::Value(5.0);
    //     item2["price"] = ctoon::Value(2.5);
    //     items.push_back(item2);
        
    //     order["items"] = ctoon::Value(items);
        
    //     ctoon::Object customer;
    //     customer["name"] = ctoon::Value("John Doe");
    //     customer["email"] = ctoon::Value("john@example.com");
    //     order["customer"] = ctoon::Value(customer);
        
    //     std::cout << ctoon::dumpsToon(ctoon::Value(order)) << std::endl;
    // }
    // std::cout << std::endl;

    // std::cout << "All examples completed successfully!" << std::endl;
    return 0;
}
