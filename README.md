# Custom Database

A relational database management system built from scratch in C++ to explore the internal architecture of modern database engines.

## Overview

Custom Database is a learning-focused DBMS project that implements many of the core components found in production databases such as PostgreSQL, MySQL, and SQLite.

The project follows a layered architecture consisting of storage management, indexing, query processing, transaction management, and networking. The goal is to gain a practical understanding of how database systems work internally rather than relying on existing database software.

---

## Features

### Storage Engine

* Page-based storage architecture
* Heap file implementation
* Disk manager for persistent storage
* Buffer pool manager
* Tuple serialization and retrieval

### Query Processing

* SQL lexical analysis
* SQL parser
* Abstract Syntax Tree (AST)
* Query planner
* Query optimizer framework
* Query executor

Supported operations include:

* CREATE TABLE
* DROP TABLE
* INSERT
* SELECT
* UPDATE
* DELETE
* DESCRIBE
* SHOW TABLES

### Indexing

* B+ Tree implementation
* Index manager
* Exact key lookups
* Range queries
* Secondary index framework

### Transaction Management

* Transaction manager
* Lock manager
* MVCC framework
* Deadlock detection
* Recovery infrastructure
* Write-Ahead Logging (WAL) framework

### Networking

* TCP server implementation
* Client-server communication
* Connection management
* Custom database protocol

---

## Architecture

```text
Client
   │
   ▼
Network Layer
   │
   ▼
Query Engine
   │
   ├── Lexer
   ├── Parser
   ├── Planner
   ├── Optimizer
   └── Executor
   │
   ▼
Storage Layer
   │
   ├── Heap Files
   ├── Buffer Pool
   ├── Disk Manager
   └── Catalog
   │
   ▼
Index Layer
   │
   └── B+ Tree
   │
   ▼
Transaction Layer
   │
   ├── Lock Manager
   ├── MVCC
   ├── WAL
   └── Recovery
```

---

## Project Structure

```text
src/
├── catalog/      Metadata management
├── index/        B+ Tree indexing
├── operators/    Relational operators
├── query/        SQL processing pipeline
├── server/       Network server
├── storage/      Storage engine
└── txn/          Transaction management

tools/
└── client.cpp    Database client

tests/
└── Unit tests
```

---

## Learning Objectives

This project was built to understand:

* Database internals
* Storage engine design
* Query execution pipelines
* B+ Tree indexing
* Buffer pool management
* Transaction processing
* Concurrency control
* Recovery mechanisms
* Network programming
* Systems programming in C++

---

## Technologies

* C++17
* CMake
* Winsock
* STL
* File I/O
* Multithreading primitives

---

## Future Work

* Cost-based query optimizer
* Join optimization
* Checkpointing
* Replication
* Distributed storage
* Query statistics
* Authentication system
* Web dashboard
* Performance benchmarking

---

## Why This Project?

Most database projects stop at implementing a key-value store. This project aims to go further by recreating the major subsystems of a real relational DBMS, including query processing, indexing, storage management, transactions, and networking.

The focus is educational: understanding how production-grade databases are engineered from the ground up.

---

## Author

Vinayak Bhumbla

Computer Science Student | Systems Programming | Databases | Cybersecurity | Distributed Systems
