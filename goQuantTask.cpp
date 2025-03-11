#define ASIO_STANDALONE
#define WIN32_LEAN_AND_MEAN 
#include <windows.h>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include "nlohmann/json.hpp"
#include "include/curl/curl.h"
#include "websocketpp/client.hpp"
#include "websocketpp/config/asio_client.hpp"
#include <asio/ssl.hpp>  
#include <set>

using json = nlohmann::json;
using websocketpp::connection_hdl;

const std::string API_KEY = "TcRbHGYQ";
const std::string API_SECRET = "2gANHl8R94BRPqG2ygg02MTBRKGm8rrOItyTpVTBeDw";
const std::string BASE_URL = "https://test.deribit.com/api/v2/";

//--------------------------------------------------------------------------------------------------------------//
//                                         DERIBIT WEBSOCKET CLASS
//--------------------------------------------------------------------------------------------------------------//
class DeribitWebSocket {
    private:
        websocketpp::client<websocketpp::config::asio_tls_client> wsClient;
        websocketpp::connection_hdl wsHandle;
        std::string wsURL = "wss://test.deribit.com/ws/api/v2";
        std::thread wsThread;
        bool running = false;
        std::string subscribedSymbol;
    
    public:
        DeribitWebSocket() {
            wsClient.init_asio();
    
            // Ensure TLS handler is set globally
            wsClient.set_tls_init_handler([](websocketpp::connection_hdl) {
                auto ctx = websocketpp::lib::make_shared<asio::ssl::context>(asio::ssl::context::tls_client);
                ctx->set_options(asio::ssl::context::default_workarounds |
                                 asio::ssl::context::no_sslv2 |
                                 asio::ssl::context::no_sslv3 |
                                 asio::ssl::context::no_tlsv1 |
                                 asio::ssl::context::no_tlsv1_1);
                return ctx;
            });
    
            wsClient.set_open_handler([this](connection_hdl hdl) {
                std::cout << "Connected to Deribit WebSocket!\n";
                wsHandle = hdl;
                subscribeToSymbol(subscribedSymbol);
            });
    
            

            wsClient.set_message_handler([this](websocketpp::connection_hdl hdl, websocketpp::config::asio_client::message_type::ptr msg) {
                try {
                    std::string jsonPayload = msg->get_payload();
            
                    
                    if (jsonPayload.empty()) {
                        std::cerr << "payload is empty\n";
                        return;
                    }
            
                    // Parse JSON
                    json response = json::parse(jsonPayload);
            
                    if (response.contains("params") && response["params"].contains("data")) {
                        json data = response["params"]["data"];
                        std::string instrument = data["instrument_name"];
                        std::string updateType = data["type"];
            
                        std::cout << "Order Book Update [" << instrument << "] - Type: " << updateType << "\n";
            
                        if (data.contains("bids")) {
                            for (const auto& bid : data["bids"]) {
                                std::cout << "Bid [" << bid[0] << "] - Price: " << bid[1] << ", Qty: " << bid[2] << "\n";
                            }
                        }
            
                        if (data.contains("asks")) {
                            for (const auto& ask : data["asks"]) {
                                std::cout << "Ask [" << ask[0] << "] - Price: " << ask[1] << ", Qty: " << ask[2] << "\n";
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "JSON Parsing Error: " << e.what() << std::endl;
                }
            });
            
        
            wsClient.set_close_handler([this](connection_hdl) {
                std::cerr << "WebSocket Disconnected!\n";
                running = false;
            });
    
            wsClient.set_fail_handler([this](connection_hdl) {
                std::cerr << "WebSocket Connection Failed!\n";
                running = false;
            });
        }
    
        void start(const std::string& symbol) {
            if (running) {
                std::cout << "WebSocket is already running.\n";
                return;
            }
    
            running = true;
            subscribedSymbol = symbol; // Store the chosen symbol
            wsThread = std::thread([this]() {
                connectWebSocket();
            });
        }
    
        void stop() {
            if (!running) {
                std::cout << "WebSocket is not running.\n";
                return;
            }
    
            std::cout << "Stopping WebSocket streaming...\n";
            running = false;
            wsClient.stop();
            if (wsThread.joinable()) wsThread.join();
        }
    
        void connectWebSocket() {
            websocketpp::lib::error_code ec;
    
            // Ensure TLS handler is set here again before getting connection
            wsClient.set_tls_init_handler([](websocketpp::connection_hdl) {
                auto ctx = websocketpp::lib::make_shared<asio::ssl::context>(asio::ssl::context::tls_client);
                ctx->set_options(asio::ssl::context::default_workarounds |
                                 asio::ssl::context::no_sslv2 |
                                 asio::ssl::context::no_sslv3 |
                                 asio::ssl::context::no_tlsv1 |
                                 asio::ssl::context::no_tlsv1_1);
                return ctx;
            });
    
            auto con = wsClient.get_connection(wsURL, ec);
            if (ec) {
                std::cerr << "WebSocket Connection Error: " << ec.message() << std::endl;
                return; 
            }
    
            wsClient.connect(con);
            wsClient.run();
        }
    
        void subscribeToSymbol(const std::string& symbol) {
            if (!running) {
                std::cerr << "WebSocket is not running. Cannot subscribe.\n";
                return;
            }
        
            std::string subscriptionMessage = "{"
                "\"jsonrpc\": \"2.0\","
                "\"method\": \"public/subscribe\","
                "\"id\": 0,"
                "\"params\": {"
                    "\"channels\": [\"book." + symbol + ".100ms\"]"
                "}"
            "}";
        
            websocketpp::lib::error_code ec;
            wsClient.send(wsHandle, subscriptionMessage, websocketpp::frame::opcode::text, ec);
        
            if (ec) {
                std::cerr << "Subscription Error: " << ec.message() << std::endl;
            } else {
                std::cout << "Subscription Sent Successfully!\n";
            }
        }
        
};
    
//--------------------------------------------------------------------------------------------------------------//
//                                         Callback function to handle response data
//--------------------------------------------------------------------------------------------------------------//
size_t WriteCallback(void* ptr, size_t size, size_t nmemb, std::string* data) {
    if (!data) return 0;
    data->append((char*)ptr, size * nmemb);
    return size * nmemb;
}

//--------------------------------------------------------------------------------------------------------------//
//                                         Function to perform HTTP requests
//--------------------------------------------------------------------------------------------------------------//
std::string performRequest(const std::string& url, const std::string& postData, const std::string& accessToken = "") {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize cURL.\n";
        return "";
    }

    std::string response;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    // Attach Bearer Token for authentication
    if (!accessToken.empty()) {
        std::string authHeader = "Authorization: Bearer " + accessToken;
        headers = curl_slist_append(headers, authHeader.c_str());
        std::cout << "Using Access Token: " << accessToken.substr(0, 10) << "...\n";
    } else {
        std::cerr << "No Access Token Provided! Request may fail.\n";
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "cURL request failed: " << curl_easy_strerror(res) << std::endl;
    } else {
        std::cout << "Request sent successfully to " << url << std::endl;
    }

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    return response;
}


//--------------------------------------------------------------------------------------------------------------//
//                                    Function to authenticate using API key and secret
//--------------------------------------------------------------------------------------------------------------//
std::string authenticate() {
    json payload = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "public/auth"},
        {"params", {
            {"grant_type", "client_credentials"},
            {"client_id", API_KEY},
            {"client_secret", API_SECRET},
            {"scope", "session:apiconsole-lovnycrcrua expires:2592000"}  // 🔹 Ensure correct scope
        }}
    };

    std::string response = performRequest(BASE_URL + "public/auth", payload.dump());

    if (response.empty()) {
        std::cerr << "Empty response from API.\n";
        return "";
    }

    try {
        json responseJson = json::parse(response);
        if (responseJson.contains("result") && responseJson["result"].contains("access_token")) {
            std::string token = responseJson["result"]["access_token"];
            std::cout << "Authentication successful! Token: " << token.substr(0, 10) << "...\n";
            return token;
        } else {
            std::cerr << "Authentication failed: " << responseJson.dump(4) << std::endl;
            return "";
        }
    } catch (const json::exception& e) {
        std::cerr << "JSON Parsing Error: " << e.what() << "\nResponse: " << response << std::endl;
        return "";
    }
}

//--------------------------------------------------------------------------------------------------------------//
//                                         Function to place an order
//--------------------------------------------------------------------------------------------------------------//

bool placeOrder(const std::string& accessToken, const std::string& instrument, double amount, const std::string& orderType, double price=0.0) {
    json payload = {
        {"jsonrpc", "2.0"},
        {"id", 5275},
        {"method", "private/buy"},
        {"params", {
            {"instrument_name", instrument},
            {"amount", amount},
            {"type", orderType}
        }}
    };

    if (orderType == "limit") {
        payload["params"]["price"] = price;
    }

    std::string response = performRequest(BASE_URL + "private/buy", payload.dump(), accessToken);

    if (response.empty()) {
        std::cerr << "Empty response from API.\n";
        return false;
    }

    try {
        json responseJson = json::parse(response);
        if (responseJson.contains("result")) {
            std::cout << "Order placed successfully!\n";
            return true;
        } else {
            std::cerr << "Order placement failed: " << responseJson.dump(4) << std::endl;
            return false;
        }
    } catch (const json::exception& e) {
        std::cerr << "JSON Parsing Error: " << e.what() << "\nResponse: " << response << std::endl;
        return false;
    }
}


//--------------------------------------------------------------------------------------------------------------//
//                                         Function to cancel open orders
//--------------------------------------------------------------------------------------------------------------//
void cancelOrders(const std::string& accessToken) {
    json payload = {
        {"jsonrpc", "2.0"},
        {"id", 5279},
        {"method", "private/get_open_orders"},
        {"params", {
            {"currency", "BTC"},  // Adjust this if needed (BTC, ETH, etc.)
            {"kind", "any"}       // Fetch both futures and options orders
        }}
    };

    std::string response = performRequest(BASE_URL + "private/get_open_orders", payload.dump(), accessToken);

    if (response.empty()) {
        std::cerr << "Empty response from API.\n";
        return;
    }

    try {
        json responseJson = json::parse(response);

        if (responseJson.contains("result") && !responseJson["result"].empty()) {
            std::cout << "Open orders found. Choose an order to cancel:\n";

            std::vector<std::string> orderIds;
            int index = 1;

            for (const auto& order : responseJson["result"]) {
                std::string orderId = order["order_id"];
                std::string instrument = order["instrument_name"];
                std::string direction = order["direction"];
                double price = order["price"];
                double amount = order["amount"];

                std::cout << index << ". Order ID: " << orderId 
                          << " | Instrument: " << instrument
                          << " | Side: " << direction
                          << " | Amount: " << amount
                          << " | Price: " << price << "\n";

                orderIds.push_back(orderId);
                index++;
            }

            int choice;
            std::cout << "Enter order number to cancel (0 to cancel all, -1 to exit): ";
            std::cin >> choice;

            if (choice == -1) {
                std::cout << "Order cancellation aborted.\n";
                return;
            } else if (choice == 0) {
                std::cout << "Cancelling all open orders...\n";

                json cancelAllPayload = {
                    {"jsonrpc", "2.0"},
                    {"id", 5280},
                    {"method", "private/cancel_all"},
                    {"params", {{"currency", "BTC"}}}
                };

                std::string cancelAllResponse = performRequest(BASE_URL + "private/cancel_all", cancelAllPayload.dump(), accessToken);
                json cancelAllResponseJson = json::parse(cancelAllResponse);

                if (cancelAllResponseJson.contains("result")) {
                    std::cout << "All open orders cancelled successfully!\n";
                } else {
                    std::cerr << "Failed to cancel all orders: " << cancelAllResponseJson.dump(4) << std::endl;
                }
                return;
            } else if (choice > 0 && choice <= orderIds.size()) {
                std::string selectedOrderId = orderIds[choice - 1];

                json cancelPayload = {
                    {"jsonrpc", "2.0"},
                    {"id", 5281},
                    {"method", "private/cancel"},
                    {"params", {{"order_id", selectedOrderId}}}
                };

                std::string cancelResponse = performRequest(BASE_URL + "private/cancel", cancelPayload.dump(), accessToken);
                json cancelResponseJson = json::parse(cancelResponse);

                if (cancelResponseJson.contains("result")) {
                    std::cout << "Order " << selectedOrderId << " cancelled successfully!\n";
                } else {
                    std::cerr << "Failed to cancel order " << selectedOrderId << ": " << cancelResponseJson.dump(4) << std::endl;
                }
            } else {
                std::cerr << "Invalid choice.\n";
            }

        } else {
            std::cout << "No open orders found.\n";
        }
    } catch (const json::exception& e) {
        std::cerr << "JSON Parsing Error: " << e.what() << "\nResponse: " << response << std::endl;
    }
}


//--------------------------------------------------------------------------------------------------------------//
//                                         Function to MODIFY ORDER
//--------------------------------------------------------------------------------------------------------------//

void modifyOrder(const std::string& accessToken) {
    // Fetch open orders
    json payload = {
        {"jsonrpc", "2.0"},
        {"id", 5281},
        {"method", "private/get_open_orders"},
        {"params", {
            {"kind", "any"}  // Fetch all types of open orders
        }}
    };

    std::string response = performRequest(BASE_URL + "private/get_open_orders", payload.dump(), accessToken);

    if (response.empty()) {
        std::cerr << "Empty response from API.\n";
        return;
    }

    try {
        json responseJson = json::parse(response);

        if (responseJson.contains("result") && !responseJson["result"].empty()) {
            std::cout << "Open orders:\n";

            std::vector<std::string> orderIds;
            int index = 1;

            for (const auto& order : responseJson["result"]) {
                std::string orderId = order["order_id"];
                std::string instrument = order["instrument_name"];
                double amount = order["amount"];
                std::string orderType = order["order_type"];
                double price = order.value("price", 0.0); // Only available for limit orders

                std::cout << index << ". Order ID: " << orderId
                          << " | Instrument: " << instrument
                          << " | Amount: " << amount
                          << " | Type: " << orderType
                          << (orderType == "limit" ? " | Price: " + std::to_string(price) : "")
                          << "\n";

                orderIds.push_back(orderId);
                index++;
            }

            // Select order to modify
            int choice;
            std::cout << "Enter the number of the order to modify (or 0 to cancel): ";
            std::cin >> choice;

            if (choice == 0 || choice > orderIds.size()) {
                std::cerr << "Invalid choice. Returning to menu.\n";
                return;
            }

            std::string selectedOrderId = orderIds[choice - 1];

            // Ask for new values
            double newAmount, newPrice;
            std::cout << "Enter new amount: ";
            std::cin >> newAmount;
            std::cout << "Enter new price (for limit orders only, otherwise enter 0): ";
            std::cin >> newPrice;

            // Modify order request
            json modifyPayload = {
                {"jsonrpc", "2.0"},
                {"id", 5282},
                {"method", "private/edit"},
                {"params", {
                    {"order_id", selectedOrderId},
                    {"amount", newAmount}
                }}
            };

            if (newPrice > 0) {
                modifyPayload["params"]["price"] = newPrice; // Add price only if it's a limit order
            }

            std::string modifyResponse = performRequest(BASE_URL + "private/edit", modifyPayload.dump(), accessToken);

            if (!modifyResponse.empty()) {
                json modifyResponseJson = json::parse(modifyResponse);
                if (modifyResponseJson.contains("result")) {
                    std::cout << "Order modified successfully!\n";
                } else {
                    std::cerr << "Order modification failed: " << modifyResponseJson.dump(4) << std::endl;
                }
            }

        } else {
            std::cout << "No open orders found.\n";
        }

    } catch (const json::exception& e) {
        std::cerr << "JSON Parsing Error: " << e.what() << "\nResponse: " << response << std::endl;
    }
}


//--------------------------------------------------------------------------------------------------------------//
//                                         Function to Get OrderBook
//--------------------------------------------------------------------------------------------------------------//

void getOrderBook() {
    // Select instrument
    std::string instrument;
    int instrumentChoice;
    std::cout << "\nSelect Instrument for Order Book:\n";
    std::cout << "1. BTC-PERPETUAL\n";
    std::cout << "2. BTC-21MAR25-80000-C\n";
    std::cout << "Enter your choice: ";
    std::cin >> instrumentChoice;

    switch (instrumentChoice) {
        case 1:
            instrument = "BTC-PERPETUAL";
            break;
        case 2:
            instrument = "BTC-21MAR25-80000-C";
            break;
        default:
            std::cerr << "Invalid instrument choice.\n";
            return;
    }

    // Construct request payload
    json payload = {
        {"jsonrpc", "2.0"},
        {"id", 5283},
        {"method", "public/get_order_book"},
        {"params", {
            {"instrument_name", instrument}
        }}
    };

    // Send request
    std::string response = performRequest(BASE_URL + "public/get_order_book", payload.dump());

    if (response.empty()) {
        std::cerr << "Empty response from API.\n";
        return;
    }

    try {
        json responseJson = json::parse(response);

        if (responseJson.contains("result")) {
            auto result = responseJson["result"];

            std::cout << "\nOrder Book for " << instrument << ":\n";
            std::cout << "---------------------------------------\n";

            // Display best bid (highest buy price)
            if (!result["bids"].empty()) {
                std::cout << "Best Bids (Buy Orders): \n";
                for (int i = 0; i < std::min(5, (int)result["bids"].size()); ++i) {
                    std::cout << "  Price: " << result["bids"][i][0]  // Fixed
                              << " | Amount: " << result["bids"][i][1] << "\n"; // Fixed
                }
            } else {
                std::cout << "No buy orders available.\n";
            }

            std::cout << "---------------------------------------\n";

            // Display best ask (lowest sell price)
            if (!result["asks"].empty()) {
                std::cout << "Best Asks (Sell Orders): \n";
                for (int i = 0; i < std::min(5, (int)result["asks"].size()); ++i) {
                    std::cout << "  Price: " << result["asks"][i][0]  // Fixed
                              << " | Amount: " << result["asks"][i][1] << "\n"; // Fixed
                }
            } else {
                std::cout << "No sell orders available.\n";
            }

            std::cout << "---------------------------------------\n";

        } else {
            std::cerr << "Failed to fetch order book: " << responseJson.dump(4) << std::endl;
        }

    } catch (const json::exception& e) {
        std::cerr << "JSON Parsing Error: " << e.what() << "\nResponse: " << response << std::endl;
    }
}



//--------------------------------------------------------------------------------------------------------------//
//                                         Function to VIEW OPEN POSITIONS
//--------------------------------------------------------------------------------------------------------------//
void viewCurrentPositions(const std::string& accessToken) {
    json payload = {
        {"jsonrpc", "2.0"},
        {"id", 5285},
        {"method", "private/get_positions"},
        {"params", {
            {"kind", "any"}  // Fetch both futures and options positions
        }}
    };

    std::string response = performRequest(BASE_URL + "private/get_positions", payload.dump(), accessToken);

    if (response.empty()) {
        std::cerr << "Empty response from API.\n";
        return;
    }

    try {
        json responseJson = json::parse(response);

        if (responseJson.contains("result") && !responseJson["result"].empty()) {
            std::cout << "\n Current Open Positions:\n";
            std::cout << "---------------------------------------\n";

            for (const auto& position : responseJson["result"]) {
                std::string instrument = position["instrument_name"];
                double size = position["size"];
                std::string direction = position["direction"];
                double entryPrice = position["average_price"];
                double markPrice = position["mark_price"];
                double floatingPL = position["floating_profit_loss"];

                std::cout << "Instrument: " << instrument << "\n";
                std::cout << "Size: " << size << " (" << direction << ")\n";
                std::cout << "Entry Price: " << entryPrice << "\n";
                std::cout << "Mark Price: " << markPrice << "\n";
                std::cout << "Floating P/L: " << floatingPL << "\n";
                std::cout << "---------------------------------------\n";
            }
        } else {
            std::cout << "No open positions found.\n";
        }
    } catch (const json::exception& e) {
        std::cerr << "JSON Parsing Error: " << e.what() << "\nResponse: " << response << std::endl;
    }
}


//--------------------------------------------------------------------------------------------------------------//
//                                         SUB-MENU Function FOR WEBSOCKET OPERATIONS
//--------------------------------------------------------------------------------------------------------------//
void websocketMenu() {
    int wsChoice;
    std::string symbol;

    // Ask user to choose BTC-PERPETUAL or ETH-PERPETUAL before starting streaming
    while (true) {
        std::cout << "\nSelect a Trading Pair:\n"
                  << "1. BTC-PERPETUAL\n"
                  << "2. ETH-PERPETUAL\n"
                  << "3. Exit to Main Menu\n"
                  << "Enter your choice: ";
        std::cin >> wsChoice;

        if (wsChoice == 1) {
            symbol = "BTC-PERPETUAL";
            break;
        } else if (wsChoice == 2) {
            symbol = "ETH-PERPETUAL";
            break;
        } else if (wsChoice == 3) {
            return; // Return to main menu
        } else {
            std::cout << "Invalid choice. Try again.\n";
        }
    }

    DeribitWebSocket ws;  // WebSocket client instance
    while (true) {
        std::cout << "\nWebSocket Menu:\n"
                  << "1. Start Market Data Streaming (" << symbol << ")\n"
                  << "2. Stop Market Data Streaming\n"
                  << "3. Exit to Main Menu\n"
                  << "Enter your choice: ";
        std::cin >> wsChoice;

        switch (wsChoice) {
            case 1:
                std::cout << "Starting Market Data Streaming for " << symbol << "...\n";
                ws.start(symbol);
                break;
            case 2:
                std::cout << "Stopping Market Data Streaming...\n";
                ws.stop();
                break;
            case 3:
                std::cout << "Returning to Main Menu...\n";
                ws.stop();  // Ensure WebSocket is closed before exiting
                return;
            default:
                std::cout << "Invalid choice. Try again.\n";
        }
    }
}



//--------------------------------------------------------------------------------------------------------------//
//                                                  MAIN MENU 
//--------------------------------------------------------------------------------------------------------------//
void mainMenu() {
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // Authenticate and get access token
    std::string accessToken = authenticate();
    if (accessToken.empty()) {
        std::cerr << "Exiting due to authentication failure.\n";
        curl_global_cleanup();
        // return 1;
    }

    // Menu-driven interface
    int choice;
    std::string instrument;
    double amount, price;
    std::string orderType;
    // DeribitWebSocket ws; 
    bool running1 = true;

    while (running1) {
        std::cout << "\nMenu:\n";
        std::cout << "1. Place Order\n";
        std::cout << "2. Cancel Order\n";
        std::cout << "3. Modify Order\n"; 
        std::cout << "4. Get Order Book\n";
        std::cout << "5. View Current Positions\n"; 
        std::cout << "6. Websocket Operations\n";
        std::cout << "7. Exit\n";
        std::cout << "Enter your choice: ";
        std::cin >> choice;

        switch (choice) {
            case 1: { 
                // Place Order
                int instrumentChoice;
                std::cout << "\nSelect Instrument:\n";
                std::cout << "1. BTC-PERPETUAL\n";
                std::cout << "2. BTC-21MAR25-80000-C\n";
                std::cout << "Enter your choice: ";
                std::cin >> instrumentChoice;

                switch (instrumentChoice) {
                    case 1:
                        instrument = "BTC-PERPETUAL";
                        break;
                    case 2:
                        instrument = "BTC-21MAR25-80000-C";
                        break;
                    default:
                        std::cerr << "Invalid instrument choice.\n";
                        continue;
                }

                std::cout << "Enter amount: ";
                std::cin >> amount;

                // Select order type
                int orderTypeChoice;
                std::cout << "Select Order Type:\n";
                std::cout << "1. Market\n";
                std::cout << "2. Limit\n";
                std::cout << "Enter choice: ";
                std::cin >> orderTypeChoice;

                if (orderTypeChoice == 1) {
                    orderType = "market";
                    placeOrder(accessToken, instrument, amount, orderType);
                } else if (orderTypeChoice == 2) {
                    orderType = "limit";
                    std::cout << "Enter Limit Price: ";
                    std::cin >> price;
                    placeOrder(accessToken, instrument, amount, orderType, price);
                } else {
                    std::cerr << "Invalid order type choice.\n";
                }
                break;
            }
            
            case 2:
                cancelOrders(accessToken);
                break;

            case 3:
                modifyOrder(accessToken);  
                break;
            
            case 4:
                getOrderBook();
                break;
            
            case 5: 
                viewCurrentPositions(accessToken);
                break;

            case 6:
                websocketMenu();  // Call WebSocket Sub-menu
                break;

            case 7: 
                std::cout << "Exiting...\n";
                curl_global_cleanup();
                running1 = false;
                break;
            
            default:
                std::cerr << "Invalid choice. Please try again.\n";
                break;
        }
    }
}


int main() {
    mainMenu();
    return 0;
}


//--------------------------------------------------------------------------------------------------------------//
//                                         STEPS FOR COMPILE & RUN the Code
//--------------------------------------------------------------------------------------------------------------//
// 1. Before compiling, open MSYS2 MINGW64 and move to your project's working directory.

// 2. Open MSYS2 MINGW64 Terminal.

// 3. Compile the Code with (g++ -g -o main main.cpp -I. -Iinclude -I/mingw64/include -L/mingw64/lib -lssl -lcrypto -lcurl -lws2_32 -lpthread
    // -g → Enables debugging symbols
    // -o main → Specifies the output executable name
    // -I. → Includes the current directory
    // -Iinclude → Includes the "include" directory
    // -I/mingw64/include → Includes system headers
    // -L/mingw64/lib → Links libraries from MinGW64
    // -lssl -lcrypto -lcurl -lws2_32 -lpthread → Links OpenSSL, cURL, Winsock, and pthread libraries
// 4. Run the Executable (./main)





//--------------------------------------------------------------------------------------------------------------//
//                                              CODED BY YASH PANCHAL
//--------------------------------------------------------------------------------------------------------------//
