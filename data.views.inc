<?php
// $Id$

/**
 * @file
 * Views hooks.
 */

/**
 * Implementation of hook_views_data().
 */
function data_views_data() {
  $data = array();
  return $data;

  $tables = flatstore_get_tables();
  foreach ($tables as $table_name => $table_schema) {
    list($database, $table_name) = flatstore_parse($table_name);

    $table = array();
    $slot = flatstore_load($table_name, $database);
    $field_meta_info = $slot->meta_info['fields'];
    foreach ($table_schema['fields'] as $field_name => $field) {
      if (!isset($field_meta_info[$field_name]['views_handler']) || $field_meta_info[$field_name]['views_handler']) {
        // Use label field from meta info if available.
        if (!empty($field_meta_info[$field_name]['label'])) {
          $title = $field_meta_info[$field_name]['label'];
        }
        else if (!empty($field['description'])) {
          $title = $field['description'];
        }
        else {
          $title = $field_name;
        }
        $table[$field_name] = array(
          'title' => $title,
          'help' => $title,
          'field' => array(
            'handler' => _flatstore_views_field_handler($field['type']),
            'help' => $title,
            'click sortable' => TRUE,
          ),
          'filter' => array(
            'handler' => _flatstore_views_filter_handler($field['type']),
            'allow empty' => TRUE,
            'help' => t('Filter on %field', array('%field' => $title)),
          ),
          'argument' => array(
            'handler' => _flatstore_views_field_argument($field['type']),
            'help' => $title,
          ),
          'sort' => array(
            'handler' => _flatstore_views_field_sort($field['type']),
            'help' => t('Sort by %field', array('%field' => $title)),
          ),
        );
      }
    }

    // Table group name.
    $table['table'] = array(
      'group' => $slot->title,
      );

    // If this table has a left table, explain how to join to it.
    // @TODO: So... there's no way for us to stop Views integration from being
    // generated from Cross tables (ie. B of A > B > C). We should be able to tell.
    if ($left_relations = flatstore_get_left_relations($table_name, $database)) {

      $table['table']['join'] = array();
      foreach ($left_relations as $relations) {
        $relation = array_pop($relations);
        if (count($relations) == 0) {
          $table['table']['join'][$relation->left_table] = array(
            'left_field' => $relation->left_col,
            'field' => $relation->right_col,
            'type' => $relation->inner_join ? 'INNER' : 'LEFT',
            'database' => $database,
          );
        }
        else if (count($relations) === 1) {
          $cross_relation = array_pop($relations);
          $table['table']['join'][$cross_relation->left_table] = array(
            'left_table' => $relation->left_table,
            'left_field' => $relation->left_col,
            'field' => $relation->right_col,
            'type' => $relation->inner_join ? 'INNER' : 'LEFT',
            'database' => $database,
          );
        }
      }
    }

    // Tables with a single primary key are base tables.
    if (count($table_schema['primary key']) >= 1) {
      $table['table']['base'] = array(
        'field' => current($table_schema['primary key']),
        'title' => $slot->title,
        'help' => t('Flat Store table'),
        'weight' => 10,
        'database' => $database,
      );
    }
    // We should never get here.
    if (!isset($table['table']['join']) && !isset($table['table']['base'])) {
      drupal_set_message(t('Table !table does not have a base table and is not a base table itself.', array('!table' => $table_name)), 'error');
      continue;
    }

    $data[$table_name] = $table;
  }
  return $data;
}

/**
 * Return a views handler string for a given field type.
 *
 * @param $field_type
 *   Schema API field type.
 * @return
 *   Views handler string.
 */
function _flatstore_views_filter_handler($type) {
  switch ($type) {
    case 'float':
    case 'numeric':
      return 'views_handler_filter_float';
    case 'int':
    case 'serial':
      return 'views_handler_filter_numeric';
    case 'datetime':
      // @TODO: make this conditional on whether the time is stored as a timestamp/datetime
      if (module_exists('date_api')) {
        return 'date_api_filter_handler';
      }
      return 'views_handler_filter_date';
  }
  return 'views_handler_filter_string';
}

// Return a field handler string for a given field type.
function _flatstore_views_field_handler($type) {
  switch ($type) {
    case 'int':
    case 'float':
    case 'serial':
    case 'numeric':
      return 'views_handler_field_numeric';
    case 'datetime':
      return 'views_handler_field_date';
  }
  return 'views_handler_field';
}

// Return a argument handler string for a given field type.
function _flatstore_views_field_argument($type) {
  switch ($type) {
    case 'int':
    case 'float':
    case 'serial':
    case 'numeric':
      return 'views_handler_argument_numeric';
    case 'datetime':
      return 'views_handler_argument_date';
    case 'varchar':
     return 'views_handler_argument_string';
  }
  return 'views_handler_argument';
}

// Return a sort handler string for a given field type.
function _flatstore_views_field_sort($type) {
  switch ($type) {
    case 'datetime':
      return 'views_handler_sort_date';
  }
  return 'views_handler_sort';
}

/**
 * Implementation of hook_date_api_fields().
 */
function flatstore_date_api_fields($field) {
  static $date_fields;

  if (!isset($date_fields)) {
    $date_fields = array();
    $tables = flatstore_get_tables();
    foreach ($tables as $table_name => $table_schema) {
      $table = array();
      $slot = flatstore_load($table_name, $table_schema['database']);
      foreach ($table_schema['fields'] as $field_name => $f) {
        if ($f['type'] == 'datetime') {
          $date_fields["{$table_name}.{$field_name}"] = $f['type'];
        }
      }
    }
  }

  $values = array(
    // The type of date: DATE_UNIX, DATE_ISO, DATE_DATETIME.
    'sql_type' => DATE_DATETIME,
    // Timezone handling options: 'none', 'site', 'date', 'utc'.
    'tz_handling' => 'site',
    // Needed only for dates that use 'date' tz_handling.
    'timezone_field' => '',
    // Needed only for dates that use 'date' tz_handling.
    'offset_field' => '',
    // Array of "table.field" values for related fields that should be
    // loaded automatically in the Views SQL.
    'related_fields' => array(),
    // Granularity of this date field's db data.
    'granularity' => array('year', 'month', 'day', 'hour', 'minute', 'second'),
  );
  if (!empty($date_fields[$field])) {
    return $values;
  }
  return array();
}

