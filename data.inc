<?php
// $Id$
/**
 * @file
 * Contains class definition for DataTable.
 */

/**
 * Manages data access and manipulation for a single data table.
 * Use data_create_table() or data_get_table() to instantiate an object from this class.
 * 
 * @see data_create_table().
 * @see data_get_table().
 * 
 * Usage:
 * 
 * Get an existing table.
 * 
 * $table = data_get_table('my_table');
 * 
 * If the table does not exist, create one.
 * if (!$table) {
 *  $table = data_create_table('my_table', $schema);
 * }
 * 
 * Save some data to it.
 * $table->save($data);
 * 
 * Remove the data from the table.
 * $table->truncate();
 * 
 * Remove the table, but not the meta information about the table.
 * data_drop_table('my_table');
 * 
 */
class DataTable {

  // Class variables.
  // @todo: change $table_schema to $schema.
  // @todo: change $name to $id.
  // Unfortunately drupal_write_record does not escape field names. $table_schema instead of $schema it is.
  protected $name, $title, $table_schema, $meta, $export_type;

  /**
   * Constructor.
   */
  public function __construct($name, $table_schema = NULL, $title = NULL) {
    if ($table_schema) {      
      $table = array(
        'name' => $name,
        'title' => empty($title) ? data_natural_name($name) : $title,
        'table_schema' => array(),
        'meta' => array(),
      );
      drupal_write_record('data_tables', $table);

      $this->init($name);
      $this->create($table_schema);
    }
    else {
      $this->init($name);
    }
  }

  /**
   * Load DataTable data from DB.
   */
  protected function init($name) {
    if ($table = _data_load_table($name)) {
      foreach (array('title', 'name', 'table_schema', 'meta', 'export_type') as $key) {
        $this->$key = $table->$key;
      }
      return TRUE;
    }
    return FALSE;
  }

  /**
   * Create a table.
   */
  protected function create($table_schema) {
    // Create table.
    db_create_table($ret, $this->name, $table_schema);
    if ($ret[0]['success'] != 1) {
      drupal_set_message(t('Error creating table.'), 'error');
      return FALSE;
    }

    // If schema module is enabled, inspect and read back to make
    // sure our schema information is up to date. 
    // @todo: this is slow, maybe we need to make this an explicit method 
    // on DataTable.
    if (module_exists('schema')) {
      $schema = schema_invoke('inspect');
      if (isset($schema[$this->name])) {
        $table_schema = $schema[$this->name];
      }
    }

    // Update table_schema information.
    $this->update(array('table_schema' => $table_schema));

    // Clear cache.
    drupal_get_schema($this->name, TRUE);

    return TRUE;
  }

  /**
   * Get a property of the DataTable object.
   * 
   * @param $property
   *   One of 'name', 'title', 'table_schema', 'meta'.
   * @return 
   *   The unserialized value of the property.
   */
  public function get($property) {
    if (in_array($property, array('name', 'title', 'table_schema', 'meta', 'export_type'))) {
      return $this->$property;
    }
  }

  /**
   * Update table properties.
   * 
   * @param $properties 
   *   Array where the key designates a property (one of 'name', 'title', 'table_schema', 'meta') 
   *   and the value is the unserialized value that this property should attain.
   */
  public function update($properties) {
    _data_override($this->name);
    $properties['name'] = $this->name;
    if (drupal_write_record('data_tables', $properties, 'name')) {
      foreach ($properties as $key => $value) {
        $this->$key = $value;
      }
    }
  }

  /**
   * Compare this table's schema to schema of table in DB. 
   * Requires schema module.
   * 
   * @return 
   *   
   */
  public function compareSchema() {
    if (module_exists('schema')) {
      $this->table_schema['name'] = $this->name;
      return schema_compare_table($this->table_schema);
    }
  }

  /**
   * Adjust table.
   * 
   * Only adds new columns at the moment: no removal, no indexes.
   * 
   * @todo: should be adjust() protected and users call update(array('table_schema' =>)) instead?
   */
  public function adjust($new_schema) {
    if ($schema = drupal_get_schema($this->name)) {
      if (isset($new_schema['fields'])) {

        foreach ($new_schema['fields'] as $field => $spec) {
          if (!isset($new_schema['fields'])) {
            $this->addField($field, $spec);
          }
        }
        if (count($ret)) {
          drupal_get_schema($this->name, TRUE);
        }
        return;
      }
    }
    $this->create($new_schema);
  }

  /**
   * Add a field.
   * 
   * @todo: Check wether field name is available, otherwise change.
   * @todo: Return false if not successful.
   */
  public function addField($field, $spec) {

    $ret = array();
    db_add_field($ret, $this->name, $field, $spec);

    if ($ret[0]['success']) {
      $schema = $this->table_schema;
      $schema['fields'][$field] = $spec;
      $this->update(array('table_schema' => $schema));
  
      // @todo: maybe move these cache clearing functions to their own method
      // and let take API users take care of caching.
      drupal_get_schema($this->name, TRUE);
      return $field;
    }
    return FALSE;
  }

  /**
   * Add an index to table.
   * 
   * @todo: support more than one field.
   */
  public function addIndex($field) {
    $schema = $this->table_schema;
    if ($schema['fields'][$field]) {
      if ($schema['fields'][$field]['type'] == 'text') {
        $index = array(array($field, 255));
      }
      else {
        $index = array($field);
      }
      db_add_index($ret, $this->name, $field, $index);
      if ($ret[0]['success']) {
        $schema['indexes'][$field] = array($field);
        $this->update(array('table_schema' => $schema));
        drupal_get_schema($this->name, TRUE);
      }
    }
  }

  /**
   * Drop an index from a table.
   */
  public function dropIndex($field) {
    $ret = array();
    db_drop_index($ret, $this->name, $field);
    if ($ret[0]['success']) {
      $schema = $this->table_schema;
      if (isset($schema['indexes'][$field])) {
        unset($schema['indexes'][$field]);
      }
      $this->update(array('table_schema' => $schema));
      drupal_get_schema($this->name, TRUE);
    }
  }

  /**
   * Change indexes of a table.
   */
  public function changeIndex($fields) {
    $schema = $this->table_schema;

    // @TODO: This array_keys() reduces indexes to single field indexes.
    // Will need adjustment when multi-field indexes are implemented.
    $indexes = isset($schema['indexes']) ? array_keys($schema['indexes']) : array();

    $add = array_diff($fields, $indexes);
    $drop = array_diff($indexes, $fields);

    foreach ($add as $field) {
      $this->addIndex($field);
    }
    foreach ($drop as $field) {
      $this->dropIndex($field);
    }
  }

  /**
   * Add a primary key to table.
   */
  public function addPrimaryKey($fields) {
    $ret = array();
    db_add_primary_key($ret, $this->name, $fields);
    if ($ret[0]['success']) {
      $schema = $this->table_schema;
      $schema['primary key'] = $fields;
      $this->update(array('table_schema' => $schema));
      drupal_get_schema($this->name, TRUE);
    }
  }

  /**
   * Drop all primary keys from a table.
   */
  public function dropPrimaryKey() {
    $ret = array();
    db_drop_primary_key($ret, $this->name);
    if ($ret[0]['success']) {
      $schema = $this->table_schema;
      $schema['primary key'] = array();
      $this->update(array('table_schema' => $schema));
      drupal_get_schema($this->name, TRUE);
    }
  }

  /**
   * Change the primary keys of a table.
   */
  public function changePrimaryKey($fields) {
    $schema = $this->table_schema;
    if (!empty($schema['primary key'])) {
      $this->dropPrimarykey();
    }
    if (!empty($fields)) {
      $this->addPrimaryKey($fields);
    }
  }

  /**
   * Change a field.
   */
  public function changeField($field, $spec) {
    $ret = array();
    db_change_field($ret, $this->name, $field, $field, $spec);

    if ($ret[0]['success']) {
      $schema = $this->table_schema;
      $schema['fields'][$field] = $spec;
      $this->update(array('table_schema' => $schema));
  
      drupal_get_schema($this->name, TRUE);
      return TRUE;
    }
    return FALSE;
  }

  /**
   * Delete a field.
   */
  public function dropField($field) {
    $ret = array();
    db_drop_field($ret, $this->name, $field);

    if ($ret[0]['success']) {
      $schema = $this->table_schema;
      unset($schema['fields'][$field]);
      $meta = $this->meta;
      unset($meta['fields'][$field]);
      $this->update(array('table_schema' => $schema), array('meta' => $meta));
  
      drupal_get_schema($this->name, TRUE);
      return TRUE;
    }
    return FALSE;
  }

  /**
   * Drop a table. Don't call this function directly, but
   * use data_drop_table() instead.
   * 
   * Does not drop a table if its defined in code.
   * 
   * @return 
   *   TRUE if the table was dropped, FALSE otherwise.
   */
  public function drop() {
    if ($this->export_type == EXPORT_IN_DATABASE || $this->export_type == DATA_EXPORT_UNDEFINED) {
      db_drop_table($ret, $this->name);
      $this->update(array('table_schema' => array()));
      drupal_get_schema($this->name, TRUE);
  
      db_query('DELETE FROM {data_tables} WHERE name = "%s"', $this->name);
      $this->title = '';
      $this->table_schema = $this->meta = array();
      return TRUE;
    }
    return FALSE;
  }

  /**
   * Revert a table to its definition in code.
   * 
   * Does not revert a table if it is not defined in code.
   * 
   * @return
   *   TRUE if the table was reverted, FALSE otherwise.
   */
  public function revert() {
    if ($this->export_type & EXPORT_IN_CODE) {
      db_query('DELETE FROM {data_tables} WHERE name = "%s"', $this->name);
      return TRUE;
    }
    return FALSE;
  }

  /**
   * Link this table to another table. Linking a table to another one is
   * to define how data in these tables should be joined to each other.
   * 
   * There can be more than one link to the left of a table. However,
   * for views integration, only the first join created will be used.
   * 
   * @todo: Get rid of link() language, use setJoin()/removeJoin() instead.
   */
  public function link($left_table, $left_field, $field = NULL, $inner_join = TRUE) {
    if ($field == NULL) {
      $field = $left_table;
    }
    $this->meta['join'][$left_table] = array(
      'left_field' => $left_field,
      'field' => $field,
      'inner_join' => $inner_join,
    );
    $this->update(array('meta' => $this->meta));
  }

  /**
   * Unlink this table from another table.
   */
  public function unlink($left_table) {
    unset($this->meta['join'][$left_table]);
    $this->update(array('meta' => $this->meta));
  }

  /**
   * Load a record.
   */
  public function load($keys) {
    $where = $values = array();
    $fields = $this->table_schema['fields'];
    foreach ($keys as $key => $value) {
      // Return if a key does not exist.
      if (!isset($fields[$key]['type'])) {
        return FALSE;
      }
      $where[] = db_escape_string($key) ." = ". db_type_placeholder($fields[$key]['type']);
      $values[] = $value;
    }

    if (!empty($where)) {
      $result = db_query('SELECT * FROM {'. db_escape_table($this->name) .'} WHERE '. implode(' AND ', $where), $values);
      $results = array();
      while ($row = db_fetch_array($result)) {
        $results[] = $row;
      }
      return count($results) ? $results : FALSE;
    }
    return FALSE;
  }

  /**
   * Save to data table.
   */
  public function save($record, $update = array()) {
    if (count($update)) {
      $keys = array();
      foreach ($update as $key) {
        $keys[$key] = $record[$key];
      }
      // Update if available.
      if ($this->load($keys)) {
        return drupal_write_record($this->name, $record, $update);
      }
    }

    // Otherwise insert.
    return drupal_write_record($this->name, $record);
  }

  /**
   * Delete from data table.
   */
  public function delete($keys) {
    $where = $values = array();
    $fields = $this->table_schema['fields'];
    foreach ($keys as $key => $value) {
      $where[] = db_escape_string($key) ." = ". db_type_placeholder($fields[$key]['type']);
      $values[] = $value;
    }
    if (!empty($where)) {
      db_query('DELETE FROM {'. db_escape_table($this->name) .'} WHERE '. implode(' AND ', $where), $values);
    }
  }

  /**
   * Empty data table.
   */
  public function truncate() {
    db_query('TRUNCATE TABLE {'. db_escape_table($this->name) .'}');
  }
}
