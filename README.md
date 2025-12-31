# Qore ODBC Module

ODBC database driver module for the [Qore programming language](http://qore.org).

## Features

- Full ODBC 3.x API support
- Transaction management (commit, rollback)
- Prepared statements with parameter binding
- Array binding for bulk operations
- LOB (Large Object) support
- Stored procedures support
- Multiple database support (PostgreSQL, MySQL, Oracle, MS-SQL, Firebird, and more)
- Timezone-aware date/time handling
- Unicode support (UTF-8)
- Auto-reconnection support

## Requirements

### Build Requirements
- CMake 3.5 or higher
- C++11 compatible compiler (GCC, Clang, MSVC)
- Qore development headers (qore-devel) 1.14 or higher
- unixODBC development headers (unixODBC-devel)
- Doxygen (for documentation generation)

### Runtime Requirements
- Qore 1.14 or higher
- unixODBC library
- ODBC driver for your target database

## Installation

### From Source

```bash
mkdir build
cd build
cmake ..
make
sudo make install
```

### RPM-based Systems (Fedora, RHEL, CentOS, openSUSE)

```bash
# Install build dependencies
sudo dnf install gcc-c++ cmake qore-devel unixODBC-devel doxygen

# Build and install
mkdir build && cd build
cmake ..
make
sudo make install
```

### Debian/Ubuntu

```bash
# Install build dependencies
sudo apt-get install g++ cmake libqore-dev unixodbc-dev doxygen

# Build and install
mkdir build && cd build
cmake ..
make
sudo make install
```

### Building Documentation

```bash
cd build
cmake ..  # if not done before
make docs
```

Documentation will be generated in `build/docs/html/`.

## Quick Start

### Basic Connection

```qore
#!/usr/bin/env qore

%require-our
%requires odbc

# Using DSN (Data Source Name)
Datasource ds("odbc:mydsn", "username", "password");

# Or using a connection string
Datasource ds("odbc", NOTHING, NOTHING, NOTHING, NOTHING, {
    "conn": "Driver={PostgreSQL};Server=localhost;Database=mydb;",
    "USER": "username",
    "PASSWORD": "password"
});

# Execute a query
hash result = ds.select("SELECT * FROM users WHERE id = %v", 1);
printf("User: %y\n", result);

ds.commit();
```

### DSN-less Connection Examples

**PostgreSQL:**
```qore
Datasource ds("odbc", NOTHING, NOTHING, NOTHING, NOTHING, {
    "conn": "Driver={PostgreSQL Unicode};Server=localhost;Port=5432;Database=testdb;",
    "USER": "postgres",
    "PASSWORD": "secret"
});
```

**MySQL:**
```qore
Datasource ds("odbc", NOTHING, NOTHING, NOTHING, NOTHING, {
    "conn": "Driver={MySQL ODBC 8.0 Driver};Server=localhost;Database=testdb;",
    "USER": "root",
    "PASSWORD": "secret"
});
```

**MS-SQL:**
```qore
Datasource ds("odbc", NOTHING, NOTHING, NOTHING, NOTHING, {
    "conn": "Driver={ODBC Driver 17 for SQL Server};Server=localhost;Database=testdb;",
    "USER": "sa",
    "PASSWORD": "secret"
});
```

**Oracle:**
```qore
Datasource ds("odbc", NOTHING, NOTHING, NOTHING, NOTHING, {
    "conn": "Driver={Oracle 19 ODBC driver};DBQ=//localhost:1521/ORCL;",
    "USER": "scott",
    "PASSWORD": "tiger"
});
```

### Prepared Statements

```qore
SQLStatement stmt(ds);
stmt.prepare("INSERT INTO users (name, email) VALUES (%v, %v)");
stmt.bindArgs(("John Doe", "john@example.com"));
stmt.exec();
ds.commit();
```

### Batch Operations with Array Binding

```qore
list names = ("Alice", "Bob", "Charlie");
list emails = ("alice@example.com", "bob@example.com", "charlie@example.com");

ds.exec("INSERT INTO users (name, email) VALUES (%v, %v)", names, emails);
ds.commit();
```

### Type Binding

Use `odbc_bind()` for explicit type specification:

```qore
%requires odbc

# Bind a DATE type explicitly
auto date_val = odbc_bind(ODBCT_DATE, 2024-01-15);
ds.exec("INSERT INTO events (event_date) VALUES (%v)", date_val);
```

Available binding types:
- `ODBCT_SLONG`, `ODBCT_ULONG` - 32-bit integers
- `ODBCT_SSHORT`, `ODBCT_USHORT` - 16-bit integers
- `ODBCT_STINYINT`, `ODBCT_UTINYINT` - 8-bit integers
- `ODBCT_FLOAT`, `ODBCT_DOUBLE` - Floating point
- `ODBCT_DATE`, `ODBCT_TIME`, `ODBCT_TIMESTAMP` - Date/time types

## Configuration Options

| Option | Description | Default |
|--------|-------------|---------|
| `optimal-numbers` | Return numeric values as integers when possible | Yes |
| `string-numbers` | Return all numeric values as strings | No |
| `numeric-numbers` | Return numeric values as arbitrary-precision numbers | No |
| `bigint-native` | Return BIGINT as native 64-bit integer | Yes |
| `bigint-string` | Return BIGINT as string | No |
| `fractional-precision` | Fractional seconds precision (1-9) | 6 |
| `login-timeout` | Login timeout in seconds | 0 (no timeout) |
| `connection-timeout` | Connection timeout in seconds | 0 (no timeout) |
| `qore-timezone` | Server timezone (e.g., "Europe/Prague", "+02:00") | Local |
| `preserve-case` | Preserve column name case in results | No |
| `conn` | Additional ODBC connection string parameters | - |

### Setting Options

```qore
# At connection time
Datasource ds("odbc:mydsn", "user", "pass", NOTHING, NOTHING, {
    "login-timeout": 30,
    "qore-timezone": "UTC",
    "numeric-numbers": True
});

# After connection
ds.setOption("fractional-precision", 9);
```

## Error Handling

The module raises various exceptions for error conditions:

| Exception | Description |
|-----------|-------------|
| `ODBC-CONNECTION-ERROR` | Connection failure |
| `ODBC-EXEC-ERROR` | SQL execution error |
| `ODBC-BIND-ERROR` | Parameter binding error |
| `ODBC-PREPARE-ERROR` | Statement preparation error |
| `ODBC-COMMIT-ERROR` | Transaction commit failure |
| `ODBC-ROLLBACK-ERROR` | Transaction rollback failure |
| `ODBC-CONNECTION-DEAD-ERROR` | Connection lost |
| `ODBC-OPTION-ERROR` | Invalid option value |

### Error Handling Example

```qore
try {
    ds.exec("INSERT INTO users (id, name) VALUES (%v, %v)", 1, "Test");
    ds.commit();
} catch (ex) {
    printf("Error: %s: %s\n", ex.err, ex.desc);
    ds.rollback();
}
```

## Troubleshooting

### Connection Issues

1. **Verify ODBC driver installation:**
   ```bash
   odbcinst -q -d  # List installed drivers
   ```

2. **Test connection with isql:**
   ```bash
   isql -v mydsn username password
   ```

3. **Check odbc.ini configuration:**
   ```bash
   cat /etc/odbc.ini      # System DSNs
   cat ~/.odbc.ini        # User DSNs
   ```

### Common Issues

- **"Driver not found"**: Install the appropriate ODBC driver for your database
- **"Connection timeout"**: Check network connectivity and firewall settings
- **"Authentication failed"**: Verify username/password and database permissions

## Links

- [Qore Programming Language](http://qore.org)
- [GitHub Repository](https://github.com/qorelanguage/module-odbc)
- [unixODBC Project](http://www.unixodbc.org)
- [API Documentation](https://qorelanguage.github.io/module-odbc)

## License

MIT License - see [LICENSE.txt](LICENSE.txt) for details.

## Author

Ondrej Musil (Qore Technologies, s.r.o.)
