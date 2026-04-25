-- To use this file, type: sudo {mysql|mariadb} mysql < mc.sql

CREATE DATABASE IF NOT EXISTS mythconverg;
-- Note that MySQL will default to caching_sql2_password while MariaDB will
-- use mysql_native_password.
CREATE USER IF NOT EXISTS 'mythtv'@'localhost' IDENTIFIED BY 'mythtv';
GRANT ALL ON mythconverg.* TO 'mythtv'@'localhost';
FLUSH PRIVILEGES;
GRANT CREATE TEMPORARY TABLES ON mythconverg.* TO mythtv@localhost;
FLUSH PRIVILEGES;
ALTER DATABASE mythconverg DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci;

-- Best to make a copy of this file (for example: my.mc.sql~) and customize it
-- for your system if any of the following are needed.

-- For remote access, add your choices of all hosts (%), a hostname, IP
-- or partial IP (e.g 10.10.10.%):
-- CREATE USER IF NOT EXISTS 'mythtv'@'%' IDENTIFIED BY 'mythtv';
-- GRANT ALL ON mythconverg.* TO 'mythtv'@'%';"

-- To convert from mysql_native_password to caching_sha2_password. Repeat for
-- all users (this -- is for MySQL only, not MariaDB):
-- ALTER USER 'mythtv'@'localhost' IDENTIFIED WITH caching_sha2_password
--     BY 'mythtv';
-- ALTER USER 'mythtv'@'%' IDENTIFIED WITH caching_sha2_password BY 'mythtv';

-- vim: set colorcolumn=80:
