This module integrates [Paystack](https://paystack.com/) into
[Drupal Commerce](https://www.drupal.org/project/commerce) payment.
It currently supports the standard and inline workflows from Paystack.

The module also support webhooks from Paystack

The initial development of this module was sponsored by
[Nosh n Nibble](https://noshnnibble.com/)

**Installation and configuration**

- Install and enable [Drupal Commerce](https://www.drupal.org/project/commerce)
and its sub-modules. See [here](https://www.drupal.org/node/120641) for how to
install a drupal module
- Install and enable
[Commerce Paystack](https://www.drupal.org/project/commerce_paystack)
- Configure the module with your paystack credentials by going to
`admin/commerce/config/payment-methods/manage/commerce_payment_paystack`
and click *Edit* beside *Enable payment method: PayStack* action
- Select the payment flow ie *Inline* or *Standard*
- Enter your secret key and public key.

**Webhook configuration**

On your Paystack [dashboard](https://dashboard.paystack.co/#/settings/developer)
set your webhook URL to `yoursite.com/commerce_paystack/webhook`
