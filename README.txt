$Id$

Data module
===========

The Data module provides

* An API for dynamically allocating tables for single-row records
* An API for insert/update/delete operations
* Automatic views integration
* Together with CTools module: exportable configurations
* Together with schema module: schema inspection and fixing

Its companion Data UI provides

* UI to add new database tables
* UI to add or alter columns to existing tables managed by Data module
* Default views for tables managed by Data module

RECOMMENDATIONS
===============

Check out FeedAPI Data module for using Data as a storage engine for feed items.

While data module does not have any dependencies, its full functionality is only
available with Schema module and CTools. 

If you use Schema module, apply latest patch from: http://drupal.org/node/533596

DEVELOPMENT
===========

This module is currently under development. Use with caution.