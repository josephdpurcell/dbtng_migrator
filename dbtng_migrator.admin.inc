<?php

/**
 * Form function, called by drupal_get_form()
 * in dbtng_migrator_menu().
 */
function dbtng_migrator_check_form($form, &$form_state) {
  $form = dbtng_migrator_settings($form, $form_state);
  $form['description']['#markup'] = t('Below are the database connections defined in Drupal. You can use this interface to check the schema against a database to another assuming it is a Drupal database.');

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Check'),
    '#access' => $form['migrate_submit']['#access'],
  );
  unset($form['migrate_submit']);
  return $form;
}

/**
 * Validation callback for dbtng_migrator_check_form().
 */
function dbtng_migrator_check_form_validate($form, &$form_state) {
  $origin = $form_state['values']['migrate_origin'];
  $dest   = $form_state['values']['migrate_destination'];
  $table_prefixes = $form_state['values']['migrate_table_prefixes_' . $origin];

  // Test connectivity.
  try {
    $test_connection = "migrate_origin";
    db_set_active($origin);
    db_query('SELECT 1');
    $test_connection = "migrate_destination";
    db_set_active($dest);
    db_query('SELECT 1');
  }
  catch (Exception $e) {
    db_set_active($origin);
    form_set_error($test_connection, t("Unable to connect to database: @message", array(
      '@message' => $e->getMessage(),
    )));
    return;
  }

  // Connect to the origin and generate a list of tables in the database.
  db_set_active($origin);

  include_once DRUPAL_ROOT . '/includes/install.inc';

  // Load install files to expose hook_schema_alter hooks.
  drupal_load_updates();

  $errors = array();

  $modules = module_list(TRUE);
  foreach ($modules as $module) {
    db_set_active($origin);
    $schema = drupal_get_schema_unprocessed($module);
    _drupal_schema_initialize($schema, $module, TRUE);

    // Some modules won't actually have a schema.
    if (!is_array($schema)) {
      continue;
    }

    // Remove tables to skip from schema list.
    // Note: assume $table_prefixes being empty means prefixes are not used so
    // include all tables.
    if (!empty($table_prefixes)) {
      foreach ($schema as $table => $info) {
        $table_prefix = Database::getConnection()->tablePrefix($table);
        if (!in_array($table_prefix, $table_prefixes)) {
          unset($schema[$table]);
        }
      }
    }

    // Get expected row counts.
    $expected_row_counts = [];
    foreach ($schema as $table => $info) {
      try {
        $expected_row_counts[$table] = (int) db_select($table)->countQuery()->execute()->fetchField();
      }
      catch (\Exception $e) {
        $expected_row_counts[$table] = 0;
        $errors[] = t('Could not get row count for @table on @db_key because of error: @error', array(
          '@table' => $table,
          '@db_key' => $origin,
          '@error' => $e->getMessage(),
        ));
      }
    }

    db_set_active($dest);

    try {

      // Iterate over the schema and assert the origin's schema exists on
      // the destination and that row counts match.
      foreach ($schema as $table => $info) {
        if (!db_table_exists($table)) {
          $errors[] = t('@table does not existing in @db_key', array('@table' => $table, '@db_key' => $dest));
          continue;
        }
        foreach ($info['fields'] as $name => $column) {
          if (!db_field_exists($table, $name)) {
            $errors[] = t('@table does not contain field @column in @db_key', array(
              '@table' => $table,
              '@column' => $name,
              '@db_key' => $dest,
            ));
          }
        }
        if (isset($info['indexes'])) {
          foreach ($info['indexes'] as $name => $columns) {
            if (!db_index_exists($table, $name)) {
              $errors[] = t('@table does not contain index @index in @db_key', array(
                '@table' => $table,
                '@index' => $name,
                '@db_key' => $dest,
              ));
            }
            foreach ($columns as $col) {
              if (is_array($col)) {
                $col = $col[0];
              }
              if (!db_field_exists($table, $col)) {
                $errors[] = t('Index check failure: @table does not contain field @column in @db_key', array(
                  '@table' => $table,
                  '@column' => $col,
                  '@db_key' => $dest,
                ));
              }
            }
          }
        }
        $found_row_count = (int) db_select($table)->countQuery()->execute()->fetchField();
        if ($expected_row_counts[$table] != $found_row_count) {
          $errors[] = t('Expected @expected_row_count rows in @table but found @found_row_count in @db_key.', array(
            '@expected_row_count' => $expected_row_counts[$table],
            '@found_row_count' => $found_row_count,
            '@table' => $table,
            '@db_key' => $dest,
          ));
        }
      }
    }
    catch (Exception $e) {
      $errors[] = $e->getMessage();
    }
  }

  if (count($errors)) {
    foreach ($errors as $i => $error) {
      form_set_error('migrate_check_' . $i, $error);
    }
  }
  else {
    drupal_set_message('The migration completed without any detected flaws.', 'success');
  }

  db_set_active('default');
}

/**
 * Submission handler for dbtng_migrator_check_form
 */
function dbtng_migrator_check_form_submit($form, &$form_state) {
  // This is only ever called if the validation hook passes all checks
  // so there is nothing to do here but state that the check was successful.
  drupal_set_message("Database check was successful.");
}

/**
 * Form function, called by drupal_get_form()
 * in dbtng_migrator_menu().
 */
function dbtng_migrator_settings($form, &$form_state) {
  $form['description']['#markup'] = t('Below are the database connections defined in Drupal. You can use this interface to migrate database to another assuming it is a Drupal database.');
  global $databases;
  $table_prefix_map = array();
  foreach (array_keys($databases) as $key) {
    $key = check_plain($key);

    $connection_info = Database::getConnectionInfo($key);
    $info = $connection_info['default'];

    // Make the information display safe.
    $rows[] = array($key, check_plain($info['driver']), check_plain($info['database']));
    $options[$key] = $key;

    $migrate_table_prefix_options_map[$key] = array();
    foreach ($info['prefix'] as $prefix_key => $prefix_value) {
      $prefix_value = check_plain($prefix_value);
      $migrate_table_prefix_options_map[$key][$prefix_value] = $prefix_value;
    }
  }

  if (count($options) < 2) {
    drupal_set_message(t('You must specify more than one database connection in the settings.php file.'), 'error');
  }

  $form['how_to'] = array(
    '#title' => 'Instructions',
    '#type' => 'fieldset',
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
  );
  $form['how_to']['details'] = array(
    '#markup' => '<p>' . t('To migrate this Drupal install to another database you\'ll 
                    need to add the database connection creditials to your settings.php file. Below is an example:') . '</p>',
  );
  $form['how_to']['example']['#markup'] = '<pre>
  &lt;?php
    // Settings.php contents above....

    // You\'re new database to migrate to. This will appear as "example" in the Migrator admin UI.
    $databases[\'example\'][\'default\'] = array(
      \'driver\' => \'mysql\',
      \'database\' => \'databasename\',
      \'username\' => \'username\',
      \'password\' => \'password\',
      \'host\' => \'localhost\',
      \'prefix\' => \'\',
    );
  ?&gt;
</pre>';

  $headers = array('Key', 'Driver', 'Database');
  $form['databases']['#markup'] = theme('table', array('header' => $headers, 'rows' => $rows));
  $form['migrate_origin'] = array(
    '#title' => 'Origin Database',
    '#type' => 'select',
    '#options' => $options,
    '#description' => t('The database the data will be replicated from.'),
    '#disabled' => count($options) < 2,
  );

  foreach ($migrate_table_prefix_options_map as $database => $migrate_table_prefix_options) {
    $form['migrate_table_prefixes_' . $database] = array(
      '#title' => 'Origin Table Prefixes',
      '#description' => 'You are using table prefixes, please select which ones you want to migrate.',
      '#type' => 'select',
      '#multiple' => TRUE,
      '#required' => TRUE,
      '#options' => $migrate_table_prefix_options,
      '#size' => 5,
      '#default_value' => array_keys($migrate_table_prefix_options),
      '#states' => array(
         // Only show this field when the 'toggle_me' checkbox is enabled.
         'visible' => array(
           ':input[name="migrate_origin"]' => array('value' => $database),
	 ),
      ),
    );
  }

  $form['migrate_destination'] = array(
    '#title' => 'Destination Database',
    '#type' => 'select',
    '#options' => $options,
    '#description' => t('The database the origin data will be replicated too.'),
    '#access' => count($options) > 1,
    '#default_value' => $key,
  );

  $form['migrate_submit'] = array(
    '#type' => 'submit',
    '#value' => t('Migrate'),
    '#access' => count($options) > 1,
  );
  return $form;
}

/**
 * Validation handler for form dbtng_migrator_settings.
 */
function dbtng_migrator_settings_validate($form, $form_state) {
  if ($form_state['values']['migrate_destination'] == $form_state['values']['migrate_origin']) {
    form_set_error('migrate_destination', t('Migration origin cannot be the same as the destination.'));
    return;
  }

  $origin = $form_state['values']['migrate_origin'];
  $dest   = $form_state['values']['migrate_destination'];
  $table_prefixes = $form_state['values']['migrate_table_prefixes_' . $origin];

  // Connect to the origin and generate a list of tables in the database.
  db_set_active($origin);
  include_once DRUPAL_ROOT . '/includes/install.inc';

  // Load install files to expose hook_schema_alter hooks.
  drupal_load_updates();

  $modules = module_list(TRUE);
  $tables  = array();
  foreach ($modules as $module) {
    $schema = drupal_get_schema_unprocessed($module);
    _drupal_schema_initialize($schema, $module, TRUE);

    // Some modules won't actually have a schema.
    if (!is_array($schema)) {
      continue;
    }

    // Remove tables to skip from schema list.
    // Note: assume $table_prefixes being empty means prefixes are not used so
    // include all tables.
    if (!empty($table_prefixes)) {
      foreach ($schema as $table => $info) {
        $table_prefix = Database::getConnection()->tablePrefix($table);
        if (!in_array($table_prefix, $table_prefixes)) {
          unset($schema[$table]);
        }
      }
    }

    foreach ($schema as $table => $info) {
      $tables[] = $table;
    }
  }

  // Check that those tables don't already exists in the destination.
  db_set_active($dest);
  foreach ($tables as $table) {
    if (db_table_exists($table)) {
      form_set_error('migrate_destination', t('%table already exists in destination database. DBTNG Migrator will not override existing data', array('%table' => $table)));
    }
  }
  db_set_active('default');
}

/**
 * Submission handler for form dbtng_migrator_settings.
 */
function dbtng_migrator_settings_submit($form, $form_state) {
  $origin = $form_state['values']['migrate_origin'];
  $destination = $form_state['values']['migrate_destination'];
  $table_prefixes = $form_state['values']['migrate_table_prefixes_' . $origin];

  $batch = array(
    'title' => t("Replicating Database"),
    'operations' => array(
      array('dbtng_migrator_batch_initialise_destination', array($destination)),
      array('dbtng_migrator_batch_install_schema', array($origin, $destination, $table_prefixes)),
      array('dbtng_migrator_batch_migrate_table', array($origin, $destination, $table_prefixes)),
    ),
    'finished' => 'dbtng_migrator_batch_report',
    'file' => drupal_get_path('module', 'dbtng_migrator') . '/dbtng_migrator.batch.inc',
  );
  batch_set($batch);
}
