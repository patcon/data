<?php
// $Id$

/**
 * Overview over all tables.
 */
function data_ui_overview() {
  
  $tables = data_get_all_tables();
  $rows = array();
  foreach ($tables as $table) {
    $rows[] = array(
      $table->get('name'), 
      l(t('Edit'), 'admin/content/data/'. $table->get('name')),
    );
  }
  $header = array(t('Name'), t('Operations'));
  return theme('table', $header, $rows);
}

/**
 * Form callback for main administration form.
 */
function data_ui_admin_form(&$form_state, $table) {
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
      '#options' => drupal_map_assoc(array('int', 'varchar', 'text')),
      '#default_value' => $field['type'],
    );
    $form['fields'][$field_name]['unsigned'] = array(
      '#type' => 'checkbox',
      '#default_value' => $field['unsigned'],
    );
  }

  // Add a new field.
  $form['new'] = array('#tree' => TRUE);
  $form['new']['name'] = array(
    '#type' => 'textfield',
    '#size' => 20,
  );
  $form['new']['label'] = array(
    '#type' => 'textfield',
    '#size' => 20,
  );
  $form['new']['type'] = array(
    '#type' => 'select',
    '#options' => drupal_map_assoc(array('int', 'varchar', 'text')),
  );
  $form['new']['unsigned'] = array(
    '#type' => 'checkbox',
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
function data_ui_admin_form_submit($form, &$form_state) {
  $table = $form_state['values']['table'];
  $schema = $table->get('table_schema');

  if ($form_state['clicked_button']['#value'] == t('Save')) {
    $fields = $schema['fields'];
    $new_fields = $form_state['values']['fields'];

    if (empty($form_state['values']['bulk_operation'])) {

      // Convert schema.
      foreach ($fields as $field_name => $field) {
        if ($new_spec = _data_ui_changed($new_fields[$field_name], $field)) {
          $table->changeField($field_name, $new_spec);
          drupal_set_message(t('Changed field !field_name', array('!field_name' => $field_name)));
        }
      }
    
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
              $table->dropField($field_name);
              drupal_set_message(t('Deleted field !field_name', array('!field_name' => $field_name)));
            }
          }
          break;
      }
    }
  }
  elseif ($form_state['clicked_button']['#value'] == t('Add new')) {
    $new = $form_state['values']['new'];
    $spec = data_build_field_spec($new);
    $table->addField($new['name'], $spec);
    $meta = $table->get('meta');
    $meta['fields'][$new['name']]['label'] = $new['label'];
    $table->update(array('meta' => $meta));
  }
}


/**
 * Theme data_ui_admin_form.
 */
function theme_data_ui_admin_form($form) {

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

  $header = array(t('Select'), t('Name'), t('Label'), t('Type'), t('Unsigned'));
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