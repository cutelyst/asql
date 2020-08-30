add_executable(async1 async1.cpp)
target_link_libraries(async1
    ASqlQt5::Core
    Qt5::Core
)

add_executable(transactions transactions.cpp)
target_link_libraries(transactions
    ASqlQt5::Core
    Qt5::Core
)
