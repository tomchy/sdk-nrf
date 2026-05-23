# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

# Get the output signed hex file for the given image.
# Usage:
#   get_signed_hex_file(<image> <file_to_sign>)
#
# Parameters:
#   image: Name of the image to get the hex file to sign for.
#   file_to_sign: Output variable to store the hex file to sign.
function(get_signed_hex_file image file_to_sign)
  set(file_to_digest)
  sysbuild_get(file_binary_dir IMAGE ${image} VAR APPLICATION_BINARY_DIR CACHE)
  sysbuild_get(file_to_digest IMAGE ${image} VAR RUNNERS_HEX_FILE_TO_SIGN CACHE)

  if(NOT file_to_digest)
    sysbuild_get(file_to_digest IMAGE ${image} VAR CONFIG_KERNEL_BIN_NAME KCONFIG)
    set(file_to_digest "${file_to_digest}.hex")
  endif()

  if(file_to_digest)
    cmake_path(APPEND file_binary_dir "zephyr" "${file_to_digest}" OUTPUT_VARIABLE file_to_digest)
    set(${file_to_sign} ${file_to_digest} PARENT_SCOPE)
  else()
    set(${file_to_sign} "${file_to_sign}-NOTFOUND" PARENT_SCOPE)
  endif()
endfunction()

# Generate manufacturing app TLVs to be included in the image metadata.
# Expects to be called from a sysbuild-level image signing script.
# Usage:
#   generate_manufacturing_app_tlvs_sysbuild(<manufacturing_image> <extra_imgtool_args> <extra_depends>)
#
# Parameters:
#   manufacturing_image: Name of the manufacturing image.
#   extra_imgtool_args: Output variable to store the extra imgtool arguments.
#   extra_depends: Output variable to store the extra dependencies.
function(generate_manufacturing_app_tlvs_sysbuild manufacturing_image extra_imgtool_args extra_depends)
  set(mfg_key_digest_args)
  set(mfg_key_digest_deps)

  ###
  # Inputs and outputs
  ###

  sysbuild_get(BINARY_DIR IMAGE ${manufacturing_image} VAR APPLICATION_BINARY_DIR CACHE)
  sysbuild_get(app_config_dir IMAGE ${manufacturing_image} VAR APPLICATION_CONFIG_DIR CACHE)

  cmake_path(APPEND app_config_dir "scripts" "pack_mfg_keys.py" OUTPUT_VARIABLE MFG_PACK_SCRIPT)
  cmake_path(APPEND BINARY_DIR "zephyr" "mfg_keys.bin" OUTPUT_VARIABLE MFG_KEYS_BIN)

  cmake_path(APPEND app_config_dir "keys" OUTPUT_VARIABLE MFG_KEYS_DIR)
  file(GLOB_RECURSE MFG_KEYS_INPUTS
    "${MFG_KEYS_DIR}/*.pem"
    "${MFG_KEYS_DIR}/*.bin"
    "${MFG_KEYS_DIR}/key_verification_msgs/*.msg"
  )

  if(SB_CONFIG_SECURE_BOOT_APPCORE)
    set(file_to_digest)
    get_fw_hex_file(b0 file_to_digest)

    if(file_to_digest)
      list(APPEND mfg_key_digest_args --area-digest ${file_to_digest})
      list(APPEND mfg_key_digest_deps ${file_to_digest})
    endif()

    if(SB_CONFIG_BOOTLOADER_MCUBOOT)
      set(file_to_digest)
      get_fw_hex_file(mcuboot_s1_variant file_to_digest)

      if(file_to_digest)
        list(APPEND mfg_key_digest_args --area-digest ${file_to_digest})
        list(APPEND mfg_key_digest_deps ${file_to_digest})
      endif()
    endif()
  endif()

  if(SB_CONFIG_BOOTLOADER_MCUBOOT)
    set(file_to_digest)
    get_fw_hex_file(mcuboot file_to_digest)

    if(file_to_digest)
      list(APPEND mfg_key_digest_args --area-digest ${file_to_digest})
      list(APPEND mfg_key_digest_deps ${file_to_digest})
    endif()
  endif()

  UpdateableImage_Get(images SIGNED ALL)
  foreach(image ${images})
    if("${image}" STREQUAL "${manufacturing_image}")
      continue()
    endif()

    list(APPEND mfg_key_digest_args --area-digest ${CMAKE_BINARY_DIR}/${image}.update.signed.hex)
    list(APPEND mfg_key_digest_deps ${CMAKE_BINARY_DIR}/${image}.update.signed.hex)
  endforeach()

  ###
  # Pack the keys/ directory into the mfg_keys blob.
  ###

  add_custom_command(
    OUTPUT  ${MFG_KEYS_BIN}
    COMMAND ${PYTHON_EXECUTABLE} ${MFG_PACK_SCRIPT}
            --keys-dir ${MFG_KEYS_DIR}
            --out-bin  ${MFG_KEYS_BIN}
            ${mfg_key_digest_args}
    DEPENDS ${MFG_PACK_SCRIPT} ${MFG_KEYS_INPUTS} ${mfg_key_digest_deps}
    COMMENT "Packing mfg_keys blob (${MFG_KEYS_BIN})"
  )
  add_custom_target(mfg_keys_artefact DEPENDS ${MFG_KEYS_BIN})

  set(NCS_MCUBOOT_MANUFACTURING_TLV_ID)
  sysbuild_get(NCS_MCUBOOT_MANUFACTURING_TLV_ID IMAGE ${manufacturing_image} VAR
    CONFIG_NCS_MCUBOOT_MANUFACTURING_TLV_ID KCONFIG)
  set(manufacturing_app_tlvs_args)
  list(APPEND manufacturing_app_tlvs_args
    --custom-tlv-file
    ${NCS_MCUBOOT_MANUFACTURING_TLV_ID}
    "${MFG_KEYS_BIN}"
  )

  set(${extra_imgtool_args} "${manufacturing_app_tlvs_args}" PARENT_SCOPE)
  set(${extra_depends} mfg_keys_artefact PARENT_SCOPE)
endfunction()
