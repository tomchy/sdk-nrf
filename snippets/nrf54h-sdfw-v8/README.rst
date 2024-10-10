.. _nrf54h-sdfw-v8:

Apply changes in partitions, introduced in Secure Domain Firmware v8
####################################################################

.. contents::
   :local:
   :depth: 2

Overview
********

This snipper rearranges local partitions to match with the memory map, introduced in Secure Domain Firmware v8.

Supported SoCs
**************

Currently, the following SoCs from Nordic Semiconductor are supported for use with the snippet:

* :ref:`zephyr:nrf54h20dk_nrf54h20`

.. note::
   This snippet does not work with earlier versions of Secure Domain Firmware.
   Please ensure to update the firmware on the device before applying this snippet to your application.
