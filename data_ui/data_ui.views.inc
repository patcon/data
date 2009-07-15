<?php
// $Id$

/**
 * @file
 * Views default views implementation.
 */

/**
 * Implementation of hook_default_view_views().
 */
function data_ui_views_default_views() {
  $views = array();
  return $views;

  $tables = flatstore_get_tables();
  foreach ($tables as $table_name => $table_schema) {
    list($database, $table_name) = flatstore_parse($table_name);

    $slot = flatstore_load($table_name, $database);

    // Create a basic table view, with exclusion flags, for each import table
    $view = new view;
    $view->name = "$table_name";
    $view->description = $table_name;
    $view->tag = 'flatstore';
    $view->view_php = '';

    // Try to relate a table to an existing table. If there are no related tables to the left,
    // check whether this table is a base table. If not, continue.
    // @todo: allow relationships to tables, 2 or more joins away.
    if ($relations = flatstore_get_left_relations($table_name, $database)) {
      // @todo: We are only building one default view for the next best
      // relation to the left. We should build all of them.
      $relation = array_shift(current($relations));
      if (count($tables[$database .'.'. $relation->left_table]['primary key']) >= 1) {
        $view->base_table = $relation->left_table;
      }
    }
    elseif (count($table_schema['primary key']) >= 1) {
      $view->base_table = $table_name;
    }
    if (empty($view->base_table)) {
      continue;
    }

    // @todo: maybe yes?
    $view->is_cacheable = FALSE;
    $view->api_version = 2;
    $view->disabled = FALSE;
    $handler = $view->new_display('default', 'Default', 'default');

    // Add our columns to the view's fields
    $fields = array();
    $field_meta_info = $slot->meta_info['fields'];
    foreach ($table_schema['fields'] as $field_name => $field) {
      if ((!isset($field_meta_info[$field_name]['views_handler']) || $field_meta_info[$field_name]['views_handler'])) {
        $fields[$field_name] = array(
          'label' => empty($field['description']) ? flatstore_format_name($field_name) : $field['description'],
          'id' => $field_name,
          'table' => $table_name,
          'field' => $field_name,
          'exclude' => 0,
          'relationship' => 'none',
        );
      }
    }
    $handler->override_option('fields', $fields);

    // Add a default argument for the first column.
    $first = key($table_schema['fields']);
    $handler->override_option('arguments', array(
      $first => array(
        'default_action' => 'ignore',
        'style_plugin' => 'default_summary',
        'style_options' => array(),
        'wildcard' => 'all',
        'wildcard_substitution' => 'All',
        'title' => $slot->title .' %1',
        'id' => $first,
        'table' => $table_name,
        'field' => $first,
      ),
    ));

    $handler->override_option('access', array(
      'type' => 'none',
    ));
    $handler->override_option('title', $slot->title);
    $handler->override_option('empty', 'There are no rows in this table.');
    $handler->override_option('empty_format', '1');
    $handler->override_option('items_per_page', 50);
    $handler->override_option('use_pager', '1');
    $handler->override_option('style_plugin', 'table');
    // Add $fields into the style options.
    foreach ($table_schema['fields'] as $field_name => $field) {
      $field_names[$field_name] = $field_name;
      $info[$field_name] = array(
        'sortable' => 1,
        'separator' => '',
      );
    }
    $handler->override_option('style_options', array(
      'grouping' => '',
      'override' => 1,
      'sticky' => 1,
      'order' => 'asc',
      'columns' => $field_names,
      'info' => $info,
      'default' => -1,
    ));

    // Add a page.
    $handler = $view->new_display('page', 'Page', 'page_1');
    $handler->override_option('path', flatstore_build_path($table_name, $database));
    $handler->override_option('menu', array(
      'type' => 'normal',
      'title' => flatstore_format_name($table_name),
      'description' => '',
      'weight' => '0',
      'name' => 'navigation',
    ));
    $handler->override_option('tab_options', array(
      'type' => 'none',
      'title' => '',
      'description' => '',
      'weight' => 0,
    ));

    // Add CSV output.
    if (module_exists('views_bonus_export')) {
      $handler = $view->new_display('feed', 'CSV feed', 'feed_1');
      $handler->override_option('items_per_page', 0);
      $handler->override_option('style_plugin', 'views_csv');
      $handler->override_option('style_options', array(
        'mission_description' => FALSE,
        'description' => '',
        'filename' => flatstore_build_path($table_name, $database) .'.csv',
        'quote' => 1,
        'header' => 1,
      ));
      $handler->override_option('row_plugin', '');
      $handler->override_option('path', flatstore_build_path($table_name, $database) .'.csv');
      $handler->override_option('menu', array(
        'type' => 'none',
        'title' => '',
        'description' => '',
        'weight' => 0,
        'name' => 'navigation',
      ));
      $handler->override_option('tab_options', array(
        'type' => 'none',
        'title' => '',
        'description' => '',
        'weight' => 0,
      ));
      $handler->override_option('displays', array(
        'page_1' => 'page_1',
        'default' => 0,
      ));
    }
    $views[$view->name] = $view;
  }
  // @todo: not necessary once http://drupal.org/node/360657 goes into views module.
  drupal_alter('flatstore_default_views', $views);
  return $views;
}
