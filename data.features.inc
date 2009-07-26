<?php
// $Id$
/**
 * @file
 * Features module integration.
 */

/**
 * Implementation of hook_features_export_options().
 */
function data_features_export_options() {
  $tables = data_get_all_tables();
  $titles = array();
  foreach ($tables as $table) {
    $titles[$table->get('name')] = $table->get('title');
  }
  return $titles;
}

/**
 * Implementation off hook_feature_export.
 */
function data_features_export($data, &$export, $module_name = '') {
  $pipe = array();
  $export['dependencies']['data'] = 'data';
  foreach ($data as $table_name) {
    if (data_get_table($table_name)) {
      $export['features']['data'][$table_name] = $table_name;
    }
  }
  return $pipe;
}

/**
 * Implementation of hook_features_export_render.
 */
function data_features_export_render($module_name, $data) {
  $export = array();
  $code = '';
  foreach ($data as $table_name) {
    $code .= data_export($table_name, '  ');
    $code .= "  ".'$data_tables[\''. $table_name .'\'] = $data_table;'."\n";
  }
  $code .= "\n".'  return $data_tables;';
  return array('data_default' => $code);
}