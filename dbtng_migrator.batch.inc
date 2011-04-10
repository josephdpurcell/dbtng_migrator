<?php

/**
 * Install database schema into destination database.
 */
function dbtng_migrator_batch_install_schema($origin, $destination) {
  db_set_active($origin);
  $modules = module_list(TRUE);

  foreach ($modules as $module) {
    $schema = drupal_get_schema_unprocessed($module);
    _drupal_schema_initialize($schema, $module, FALSE);

    db_set_active($destination);
    foreach ($schema as $name => $table) {
      db_create_table($name, $table);
      if (db_table_exists($name)) {
        drupal_set_message("$name was successfully created.");
      }
      else {
        drupal_set_message("$name could not be created.", 'error');
      }
    }
    db_set_active($origin);
  }

  // Set connection back to the default Drupal connection.
  db_set_active('default');
}

/**
 * Migrate data from origin to destination one table at a time.
 */
function dbtng_migrator_batch_migrate_table($origin, $destination, &$context) {
  if (empty($context['results'])) {
    db_set_active($origin);
    $modules = module_list(TRUE);
    $tables  = array();
    foreach ($modules as $module) {
      $schema = drupal_get_schema_unprocessed($module);
      _drupal_schema_initialize($schema, $module, TRUE);

      // Some modules won't actually have a schema.
      if (!is_array($schema)) {
        continue;
      }
      foreach ($schema as $table => $info) { 
        $tables[] = $table;
      }
    }
    $context['sandbox']['unprocessed_tables'] = $tables;
    $context['sandbox']['max'] = count($tables);
    $context['sandbox']['progress'] = 0;
    $context['results'] = array();
  }

  $table = array_shift($context['sandbox']['unprocessed_tables']);
  db_set_active($origin);

  $schema = drupal_get_schema($table);

  // Set a default sort as range queries must have sorts.
  $order_by = key($schema['fields']);
  if (isset($schema['primary key'])) {
    $order_by = current($schema['primary key']);
  }


  try {

    $context['message'] = t("Processing !table successful", array('!table' => $table));

    // Fix this to make work with non-SQL databases.
    $total = db_query("SELECT COUNT(*) FROM {$table}")->fetchCol();
    $count = 0;
    $limit = 500;

    while ($count < $total[0]) {
      db_set_active($origin);
      $rows = db_select($table, 't')
                ->fields('t', array_keys($schema['fields']))
                ->orderBy($order_by)
                ->range($count, $limit)
                ->execute();

      // No need to generate insert queries that don't insert anything.
      if (!count($rows)) {
        continue;
      }

      db_set_active($destination);
      $insert = db_insert($table)->fields(array_keys($schema['fields']));
      foreach ($rows as $row) {
         $insert->values((array) $row); 
      } 
      $insert->execute();
      $count += $limit;
    }
  }
  catch(Exception $e) {
    drupal_set_message($e->getMessage(), 'error');
    $context['message'] = t("Processing !table failed.", array('!table' => $table));
  }
  $context['results'][] = $table;

  $context['finished'] = empty($context['sandbox']['unprocessed_tables']);
  db_set_active('default');
}

/**
 * Generate batch report of migration.
 */
function dbtng_migrator_batch_report($success, $results) {
  if (!$success) {
    drupal_set_message("Migrator failed to successfully migrate data to destination database", 'error');
  }
  foreach ($results as $result) {
    drupal_set_message("$result successfully migrated.");
  }
}
