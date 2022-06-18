CREATE SCHEMA testlibpq3;
SET search_path = testlibpq3;
SET standard_conforming_strings = ON;
CREATE TABLE test1 (
    i int4
  , r real
  , bo boolean
  , ts timestamp
  , t text
  , b bytea
);
INSERT INTO test1
VALUES (
    1
  , 3.141593
  , true
  , '2000-01-01 00:00:02.414213'
  , 'joe''s place'
  , '\000\001\002\003\004'
);
INSERT INTO test1
VALUES (
    2
  , 1.618033
  , false
  , '2000-01-01 00:00:01.465571'
  , 'ho there'
  , '\004\003\002\001\000'
);
