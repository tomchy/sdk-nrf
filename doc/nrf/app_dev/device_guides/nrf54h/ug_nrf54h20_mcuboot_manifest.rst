.. _ug_nrf54h20_mcuboot_manifest:

Configuring nRF54H20 application for updates using a manifest
#############################################################

.. contents::
   :local:
   :depth: 2

This guide describes the manifest-based update strategy and an example of configuring an nRF54H20 application to use a merged slot for updates in the Direct XIP mode with revert.
It is organized into several key sections:

* A brief overview of the manifest-based update strategy and when it is beneficial to use it.
* A brief description of the MCUboot manifest.
* An introduction to the manifest-based update and boot strategy.
* An example, demonstrating how to perform partial updates using a manifest-based updates in Direct XIP mode with revert.

For more details on the differences between update swap and direct XIP strategies, see :ref:`ug_bootloader_main_config`.

The manifest-based update strategy is currently supported only in Direct XIP mode on nRF54H20 platform.

Overview
********

The multi-core support in MCUboot design assumes that each core has its own, independent firmware image.
As mentioned inside the :ug_nrf54h20_partitioning_merged, this leads to two complications when using the Direct XIP update strategy:

1. There is no signalling mechanism to pass verification results of the radio core firmware from the bootloader to the application core firmware.
   There is also no way to ensure that the bootloader starts all images from the same slot.
#. Each image needs to define its own set of dependencies, leading to a complex web of interdependencies between images.
   This makes it difficult to ensure compatibility between different images and manage their versions effectively.

To address these challenges, MCUboot introduces the manifest-based update strategy.
In this strategy, a single manifest TLV describes multiple firmware images through their digests, defining a set of images, that were tested together and are known to be compatible.
The manifest image is the only image that contains a manifest TLV and is the only image that needs to be confirmed or reverted.
There are two manifests inside the system: one for each manifest image slot.
The bootloader ensures that all of the images, described by the manifest are present and valid before switching to the new slot.
This ensures that all images are compatible and reduces the complexity of managing interdependencies between images.

This strategy also allows to perform partial updates of the system, with the limitation that the manifest image must always be updated.
For example, it is possible to update only the application core firmware while keeping the radio core firmware unchanged.
This is particularly useful when the application core firmware requires frequent updates, while the radio core firmware remains relatively stable.

The manifest-based update strategy is beneficial in scenarios where:

* The system consists of multiple firmware images scattered across different memory regions that need to be managed together.
* There is a need to perform partial updates of the system while ensuring compatibility between different images.
* A metered connection is used for updates, and minimizing the amount of data transferred is crucial.
* It is required to provide a precise information about all of the images running on the device for auditing or compliance purposes.
* A slot preference mechanism is needed to control which slot should be used for booting.

However, the manifest-based update strategy comes with some trade-offs:

* Increased complexity in managing manifests and ensuring that all images described by a manifest are updated together.
* Additional overhead in terms of storage space for manifests and digests.
* Currently manifests do not support describing dependencies between images using semantic versions.
* It is not possible to update any image without updating the manifest image.

MCUboot manifest
****************

The MCUboot manifest is a TLV structure that describes multiple firmware images through their digests.
The first version of the manifest contains just the digests of the images and can be represented by the following structure:

.. code-block:: c

   struct mcuboot_manifest {
        uint32_t format;
        uint32_t image_count;
        /* Skip a digest of the MCUBOOT_MANIFEST_IMAGE_INDEX image. */
        uint8_t image_hash[MCUBOOT_IMAGE_NUMBER - 1][IMAGE_HASH_SIZE];
   };

The ``format`` field specifies the format of the manifest. Currently only the version ``1`` is defined.
The ``image_count`` field specifies the number of images described by the manifest.
The ``image_hash`` array contains the cryptographic hashes of the images, excluding the manifest image itself.
The manifest image is configured by the :kconfig:`SB_CONFIG_MCUBOOT_MANIFEST_IMAGE_INDEX` symbol.
The size of each hash must be the same and is determined by the hash algorithm configured through the :kconfig:`SB_CONFIG_BOOT_IMG_HASH_ALG` choice.

The manifest structure is generated during the build process and is appended to the manifest image based on the configuration generated automatically by the build system.
The configuration file can be found in the build directory under the :file:`./build/<app_name>/zephyr/manifest.yaml` for the primary application and :file:`./build/mcuboot_secondary_app/zephyr/manifest.yaml` for the secondary application.
The contents of the configuration file reflects the fields of the manifest structure described above, for example:

.. code-block:: yaml

  format: '1'
  images:
   - name: 'ipc_radio'
     path: 'build/ipc_radio/zephyr/zephyr.signed.bin'

By default, the format uses bath to the image, so the digest is updated whenever a new manifest is generated.
It is also possible to specify the digest directly in the configuration file by using the ``hash`` field instead of the ``path`` field.
This can be useful when the image is not built as part of the same build system or when the image is not available during the manifest generation.

.. code-block:: yaml

  format: '1'
  images:
   - name: 'ipc_radio'
     hash: '3a7bd3e2360a3d485f2f75f3b70a5f8b1c9d5e6f7a8b9c0d1e2f3a4b5c6d7e8f'

The generation of the manifest structure is handled by the ``imgtool sign`` command.
The path to the manifest configuration file can be specified using the ``--manifest`` option.
The manifest TLV is appended to the image's protected TLV area, beeing included in the image's signature.
For more information about the ``imgtool`` command, see :ref:`ug_mcuboot_imgtool`.

Manifest-based update and boot strategy
***************************************

The manifest-based update and boot strategy can be enabled by setting the :kconfig:option:`SB_CONFIG_MCUBOOT_MANIFEST_UPDATES` option.
When using this strategy, the bootloader performs the following steps:

1. It looks for the best manifest image candidate slot. The selection of the preferred slot uses the following rules:
   #. If one of the slots is not valid, the other slot is selected as active.
   #. If both slots are valid, the slot marked as "preferred" is selected as active.
   #. If both slots are valid and none is marked as "preferred," the slot with the higher version number is selected as active.
   #. If none of the above conditions is met, slot A is selected as active.
#. It verifies that the manifest image is valid and copies the manifest from the TLV area to the internal bootloader state.
#. If the manifest images is valid, the bootloader checks the manifest image confirmation status.
   #. If the manifest image is pending, a dedicated flag is set to indicate that the manifest image is pending confirmation.
   #. If the manifest image is pending confirmation, the bootloader erases the manifest image, marks the selected slot as unavailable, and restarts the process from step 1.
   #. If the manifest image is confirmed, the bootloader proceeds to the next step.
#. It iterates over all other images and verifies it's validity. An image digest check is extended to verify that the digest matches the value specified in the manifest.
#. If an image is invalid, the bootloader erases the image, marks the selected slot as unavailable, and restarts the process from step 1.
#. If the manifest image is confirmed and the rollback protection is enabled, the bootloader updates the security counter value.
#. If all images are valid, the bootloader boots the selected slot.

There is no dedicated update logic.
The update process is based on the preferred slot selection.

It is important to note, that the confirmation flags are analyzed before checking the availability of other images, so the manifest image should be selected for testing once all other images are downloaded.
The readiness of the manifest image to be applied can be checked using the SMP protocol and looking at the ``bootable`` flag of the manifest image.

Performing partial updates using a manifest
*******************************************

To perform a partial update using a manifest, the following steps should be followed:
1. Prepare the new images to be updated, ensuring that they are compatible with the existing images.
2. Create a new manifest configuration file that describes all images, including the new images and the existing images that are not being updated.
3. Use the ``imgtool sign`` command to generate the new manifest image, specifying the new manifest configuration file using the ``--manifest`` option.
4. Use the ``imgtool sign`` command to generate the new images that are being updated.
5. Use the SMP protocol to upload the new manifest image and the new images to the device, ensuring that the manifest image is uploaded last.
6. Reboot the device to apply the update. 
During the update process, the bootloader will verify the manifest image and all other images, ensuring that they are valid and compatible before switching to the new slot.
For more information about using the SMP protocol for updates, see :ref:`device_mgmt`.
Example
*******

This section provides an example of configuring an nRF54H20 application to use a manifest-based update strategy in Direct XIP mode with revert.
The example assumes that the application consists of two images: an application core image and a radio core image.
The application core image is the manifest image, and the radio core image is described by the manifest.
Configuring the application
+++++++++++++++++++++++++++++++
To configure the application for manifest-based updates, the following steps should be followed:
1. Enable the :kconfig:option:`SB_CONFIG_MCUBOOT_MANIFEST_UPDATES` option in both the application core and radio core images.
2. Set the :kconfig:option:`SB_CONFIG_MCUBOOT_MANIFEST_IMAGE_INDEX` option to ``0`` in the application core image and to ``1`` in the radio core image.
3. Create a manifest configuration file for the application core image that describes both the application core image and the radio core image.
   The configuration file should look like this:
.. code-block:: yaml
  format: '1'
  images:
   - name: 'app_core'
   - name: 'radio_core'
4. Use the ``imgtool sign`` command to generate the application core image, specifying the manifest configuration file using the ``--manifest`` option.
5. Use the ``imgtool sign`` command to generate the radio core image.
6. Use the SMP protocol to upload both images to the device, ensuring that the application core image is uploaded last.
7. Reboot the device to apply the update.
During the update process, the bootloader will verify the manifest image and the radio core image, ensuring that they are valid and compatible before switching to the new slot.
For more information about using the SMP protocol for updates, see :ref:`device_mgmt`.
References
**********
* :ref:`ug_mcuboot_imgtool` - Guide to using the MCUboot image tool (imgtool).
* :ref:`device_mgmt` - Guide to device management using MCUmgr and SMP protocols.
* :ref:`ug_bootloader_main_config` - Guide to configuring the MCUboot bootloader.

