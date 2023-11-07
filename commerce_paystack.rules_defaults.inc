<?php
/**
 * @file
 * Default Rules.
 */

/**
 * Implements hook_default_rules_configuration().
 *
 * Creates the default rule that sets and remove the paystack charge.
 */
function commerce_paystack_default_rules_configuration() {
  $rule = rules_reaction_rule();
  $rule->label = 'Set Paystack Charge';
  $rule->active = TRUE;
  $rule->event('commerce_order_update')
    ->condition('commerce_payment_selected_payment_method', array('commerce_order:select' => 'commerce-order', 'method_id' => 'paystack'))
    ->action('commerce_paystack_add_paystack_charges', array('commerce_order:select' => 'commerce-order'));

  $configs['set_paystack_charge'] = $rule;

  $condition = rules_condition('commerce_payment_selected_payment_method', array('commerce_order:select' => 'commerce-order', 'method_id' => 'paystack'));
  $rule = rules_reaction_rule();
  $rule->label = 'Remove Paystack Charge';
  $rule->active = TRUE;
  $rule->event('commerce_order_update')
    ->condition($condition->negate())
    ->action('commerce_paystack_remove_paystack_charges', array('commerce_order:select' => 'commerce-order'));

  $configs['remove_paystack_charge'] = $rule;

  return $configs;
}
