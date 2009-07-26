<?php
// $Id$
/**
 * @file
 * Admin UI functions.
 */

/**
 * Overview over all tables.
 */
function data_ui_overview() {
  $tables = data_get_all_tables();
  $rows = array();
  foreach ($tables as $table) {
    // Build operations depending on configuration status.
    $operations = array();
    if ($table->get('export_type') == EXPORT_IN_CODE) {
      $status = t('Default');
      $operations[] = l(t('Override'), 'admin/content/data/edit/'. $table->get('name'));
    }
    else if ($table->get('export_type') == (EXPORT_IN_CODE | EXPORT_IN_DATABASE)) {
      $status = t('Overridden');
      $operations[] = l(t('Edit'), 'admin/content/data/edit/'. $table->get('name'));
      $operations[] = l(t('Revert'), 'admin/content/data/revert/'. $table->get('name'));
    }
    else {
      $status = t('Normal');
      $operations[] = l(t('Edit'), 'admin/content/data/edit/'. $table->get('name'));
      $operations[] = l(t('Drop'), 'admin/content/data/drop'. $table->get('name'));
    }
    if (module_exists('ctools')) {
      $operations[] = l(t('Export'), 'admin/content/data/export/'. $table->get('name'));
    }

    $row = array();
    $row[] = $table->get('name');
    $row[] = $status;
    $row[] = implode(' | ', $operations);

    $rows[] = $row;
  }
  $rows[] = array(
    l(t('Create new table'), 'admin/content/data/create'),
    '&nbsp;',
  );
  $header = array(t('Name'), t('Status'), t('Operations'));
  return theme('table', $header, $rows);
}

/**
 * Form callback for create table form.
 */
function data_ui_create_form(&$form_state) {
  $form = array();
  if (!$form_state['storage']['field_num']) {
    $form['name'] = array(
      '#type' => 'textfield',
      '#title' => t('Table name'),
      '#description' => t('Machine readable name of the table - e. g. "my_table". Must only contain lower case letters and _.'),
      '#required' => TRUE,
    );
    $form['title'] = array(
      '#type' => 'textfield',
      '#title' => t('Table title'),
      '#description' => t('Natural name of the table - e. g. "My Table".'),
    );
    $form['field_num'] = array(
      '#type' => 'textfield',
      '#title' => t('Number of fields'),
      '#description' => t('The number of fields this table should contain.'),
      '#default_value' => 1,
      '#required' => TRUE,
    );
    $form['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Next'),
    );
  }
  else {
    $form['help']['#value'] = t('Define the fields of the new table.');
    $form['fields'] = array(
      '#tree' => TRUE,
    );
    for ($i = 0; $i < $form_state['storage']['field_num']; $i++) {
      $form['fields']['field_'. $i] = _data_ui_field_form(TRUE);
    }
    $form['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Create'),
    );
  }
  return $form;
}

/**
 * Validate handler for create table form.
 */
function data_ui_create_form_validate($form, &$form_state) {
  if (data_get_table(data_name($form_state['values']['name']))) {
    form_set_error('name', t('Name is already taken.')); 
  }
  if (isset($form_state['values']['field_num'])) {
    if (is_numeric($form_state['values']['field_num'])) {
      if ($form_state['values']['field_num'] < 1) {
        form_set_error('field_num', t('At least one field must be created.')); 
      }
    }
    else {
      form_set_error('field_num', t('Enter a number greater than 0.')); 
    }
  }
  // Check for dupes.
  if (isset($form_state['values']['fields'])) {
    $names = array();
    foreach ($form_state['values']['fields'] as $field) {
      if (is_numeric($names[$field['name']])) {
        form_set_error('name', t('Names can\'t be numbers.'));
      }
      if (!isset($names[$field['name']])) {
        $names[$field['name']] = $field['name'];
      }
      else {
        form_set_error('name', t('Names must be unique.'));
      }
    }
  }
}

/**
 * Submit handler for create table form.
 */
function data_ui_create_form_submit($form, &$form_state) {

  if (isset($form_state['values']['field_num'])) {
    $form_state['storage'] = $form_state['values'];
  }
  elseif (isset($form_state['values']['fields'])) {

    // Create a schema from user input.
    $schema = $index = $primary = $meta = array();
    foreach ($form_state['values']['fields'] as $field) {
      $schema['fields'][$field['name']] = data_build_field_definition($field);
      $meta['fields'][$field['name']]['label'] = $field['label'];

      // Limit index if field type is text.
      if (!empty($field['index'])) {
        $index[$field['name']] = data_get_index_definition($field['name'], $field['type']);
      }
      if (!empty($field['primary'])) {
        $primary[] = data_get_pk_definition($field['name'], $field['type']);
      }
    }
    $schema['indexes'] = $index;
    $schema['primary key'] = $primary;

    // Create table.
    if ($table = data_create_table(data_name(trim($form_state['storage']['name'])), $schema, trim($form_state['storage']['title']))) {
      $meta = $table->update(array('meta' => $meta));
      drupal_set_message(t('Created table !table', array('!table' => $table->get('name'))));
    }
    else {
      drupal_set_message(t('Error creating table'), 'error');
    }

    // Unset storage to enable redirect.
    unset($form_state['storage']);
    $form_state['redirect'] = 'admin/content/data';
  }
}

/**
 * Form callback for revert table form.
 */
function data_ui_revert_form(&$form_state, $table) {
  $form = array();
  $form['#redirect'] = 'admin/content/data';
  $form['#table'] = $table;

  return confirm_form($form,
    t('Revert this table?'),
    'admin/content/data',
    t('Are you sure you would like to revert table !table? This will reset all information about this table its definition in code. This action cannot be undone.', array('!table' => $table->get('name'))),
    t('Revert'), t('Cancel')
  );
}

/**
 * Submit handler for data_ui_revert_form().
 */
function data_ui_revert_form_submit($form, &$form_state) {
  $table = $form['#table'];
  $table->revert();
}

/**
 * Form callback for drop table form.
 */
function data_ui_drop_form(&$form_state, $table) {
  $form = array();
  $form['#redirect'] = 'admin/content/data';
  $form['#table'] = $table;

  return confirm_form($form,
    t('Drop this table?'),
    'admin/content/data',
    t('Are you sure you would like to drop table !table? This action cannot be undone.', array('!table' => $table->get('name'))),
    t('Drop'), t('Cancel')
  );
}

/**
 * Submit handler for data_ui_drop_form().
 */
function data_ui_drop_form_submit($form, &$form_state) {
  $table = $form['#table'];
  data_drop_table($table->get('name'));
}

/**
 * Form callback for editing a table.
 */
function data_ui_edit_form(&$form_state, $table) {
  drupal_set_title(t('Data table !table', array('!table' => $table->get('name'))));
  $schema = $table->get('table_schema');
  $meta = $table->get('meta');
  
  $form = array();
  // Keep table.
  $form['table'] = array(
    '#type' => 'value',
    '#value' => $table,
  );

  // Existing fields.
  $form['fields'] = array('#tree' => TRUE);
  if (isset($schema['fields'])) {
    foreach ($schema['fields'] as $field_name => $field) {
      $form['fields'][$field_name] = array();
      $form['fields'][$field_name]['selected'] = array(
        '#type' => 'checkbox',
      );
      $form['fields'][$field_name]['name'] = array('#value' => $field_name);
      $form['fields'][$field_name]['label'] = array(
        '#type' => 'textfield',
        '#size' => 20,
        '#default_value' => $meta['fields'][$field_name]['label'],
      );
      $form['fields'][$field_name]['type'] = array(
        '#type' => 'select',
        '#options' => data_get_field_types(),
        '#default_value' => $field['type'],
      );
      $form['fields'][$field_name]['unsigned'] = array(
        '#type' => 'checkbox',
        '#default_value' => $field['unsigned'],
      );
      $form['fields'][$field_name]['index'] = array(
        '#type' => 'checkbox',
        '#default_value' => isset($schema['indexes'][$field_name]),
      );
      $form['fields'][$field_name]['primary'] = array(
        '#type' => 'checkbox',
        '#default_value' => isset($schema['primary key']) ? in_array($field_name, $schema['primary key']) : FALSE,
      );
    }
  }

  // Add a new field.
  $form['new'] = _data_ui_field_form();
  $form['new']['primary'] = array(
    '#type' => 'markup',
    '#value' => '&nbsp;',
    );
  $form['new']['add'] = array(
    '#type' => 'submit',
    '#value' => t('Add new'),
  );

  // Bulk operations.
  $options = array(
    t('Bulk operations'),
    'delete' => t('Delete all selected'),
  );
  $form['bulk_operation'] = array(
    '#type' => 'select',
    '#options' => $options,
  );
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
  );

  return $form;
}

/**
 * Submit form.
 */
function data_ui_edit_form_submit($form, &$form_state) {
  $table = $form_state['values']['table'];
  $schema = $table->get('table_schema');

  if ($form_state['clicked_button']['#value'] == t('Save')) {
    $fields = $schema['fields'];
    $new_fields = $form_state['values']['fields'];

    $new_index = array();
    $new_primary_key = array();

    if (empty($form_state['values']['bulk_operation']) && isset($fields)) {

      // Convert schema.
      foreach ($fields as $field_name => $field) {
        if ($new_spec = _data_ui_changed($new_fields[$field_name], $field)) {
          $table->changeField($field_name, $new_spec);
          drupal_set_message(t('Changed field !field_name', array('!field_name' => $field_name)));
        }
        if ($new_fields[$field_name]['index']) {
          $new_index[] = $field_name;
        }
        if ($new_fields[$field_name]['primary']) {
          $new_primary_key[] = $field_name;
        }
      }
      $table->changeIndex($new_index);
      $table->changePrimaryKey($new_primary_key);

      // Update meta data.
      $meta = $table->get('meta');
      foreach ($new_fields as $field_name => $field) {
        $meta['fields'][$field_name]['label'] = $field['label'];
      }
      $table->update(array('meta' => $meta));
    }
    else {
      // Bulk updates.
      switch ($form_state['values']['bulk_operation']) {
        case 'delete':
          foreach ($new_fields as $field_name => $field) {
            if (!empty($field['selected'])) {
              // One field must stay.
              $schema = $table->get('table_schema');
              if (count($schema['fields']) > 1) {
                $table->dropField($field_name);
                drupal_set_message(t('Deleted field !field_name', array('!field_name' => $field_name)));
              }
              else {
                drupal_set_message('You cannot delete all fields from a table, drop the table instead.', 'error');
              }
            }
          }
          break;
      }
    }
  }
  elseif ($form_state['clicked_button']['#value'] == t('Add new')) {
    $new = $form_state['values']['new'];
    $spec = data_build_field_definition($new);
    $table->addField($new['name'], $spec);
    if (!empty($new['index'])) {
      $table->addIndex($new['name']);
    }
    $meta = $table->get('meta');
    $meta['fields'][$new['name']]['label'] = $new['label'];
    $table->update(array('meta' => $meta));
  }
}

/**
 * Export form.
 */
function data_ui_export_form(&$form_state, $table) {
  $code = data_export($table->get('name'));

  $form['export'] = array(
    '#title' => t('Export table definition'),
    '#type' => 'textarea',
    '#value' => $code,
    '#rows' => substr_count($code, "\n"),
  );
  return $form;
}

/**
 * Theme data_ui_create_form.
 */
function theme_data_ui_create_form($form) {

  // Render field definition form elements in a table.
  if (isset($form['fields'])) {
    $output = drupal_render($form['help']);

    $rows = array();
    foreach (element_children($form['fields']) as $e) {
      $row = array();
      foreach (element_children($form['fields'][$e]) as $f) {    
        $row[] = drupal_render($form['fields'][$e][$f]);
      }
      $rows[] = $row;
    }
    $header = array(t('Name *'), t('Label'), t('Type'), t('Unsigned'), t('Index'), t('Primary key'));
    $output .= theme('table', $header, $rows);

    $output .= drupal_render($form);
    return $output;
  }
  return drupal_render($form);
}

/**
 * Theme data_ui_admin_form.
 */
function theme_data_ui_edit_form($form) {

  // Format existing fields.
  $rows = array();
  foreach (element_children($form['fields']) as $e) {
    $row = array();
    foreach (element_children($form['fields'][$e]) as $f) { 
      $row[] = drupal_render($form['fields'][$e][$f]);
    }
    $row[] = '&nbsp;';
    $rows[] = $row;
  }

  // New fields form.
  $row = array('&nbsp;');
  foreach (element_children($form['new']) as $e) {
    $row[] = drupal_render($form['new'][$e]);
  }
  $rows[] = $row;

  $header = array(t('Select'), t('Name'), t('Label'), t('Type'), t('Unsigned'), t('Index'), t('Primary key'));
  $output .= theme('table', $header, $rows);
  $output .= drupal_render($form);
  return $output;
}

/**
 * Magic helper function. Detect changed between keys in $new and $field
 * and return a new field spec based on $field IF there are differences.
 * 
 * Otherwise return FALSE.
 * 
 * Currently checked: type, unsigned
 */
function _data_ui_changed($new, $field) {
  $changed = FALSE;
  if ($field['type'] != $new['type']) {
    $field['type'] = $new['type'];
    $changed = TRUE;
  }
  if ($field['unsigned'] != $new['unsigned']) {
    $field['unsigned'] = $new['unsigned'];
    $changed = TRUE;
  }
  if ($changed) {
    return $field;
  }
  return FALSE;
}

/**
 * Helper function that generates a form snippet for defining a field.
 */
function _data_ui_field_form($required = FALSE) {
  $form = array();
  $form['#tree'] = TRUE;
  $form['name'] = array(
    '#type' => 'textfield',
    '#size' => 20,
    '#required' => $required,
  );
  $form['label'] = array(
    '#type' => 'textfield',
    '#size' => 20,
  );
  $form['type'] = array(
    '#type' => 'select',
    '#options' => data_get_field_types(),
  );
  $form['unsigned'] = array(
    '#type' => 'checkbox',
  );
  $form['index'] = array(
    '#type' => 'checkbox',
  );
  $form['primary'] = array(
    '#type' => 'checkbox',
  );
  return $form;
}
