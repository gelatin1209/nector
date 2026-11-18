if(NOT DEFINED CMAKE_INSTALL_PREFIX)
    message(FATAL_ERROR "CMAKE_INSTALL_PREFIX is required to generate the Nector package manifest")
endif()

file(TO_CMAKE_PATH "${CMAKE_INSTALL_PREFIX}" nector_package_root)
file(GLOB_RECURSE nector_manifest_files LIST_DIRECTORIES false
    RELATIVE "${nector_package_root}" "${nector_package_root}/*")
list(FILTER nector_manifest_files EXCLUDE REGEX "^metadata/(manifest\\.json|checksums\\.sha256)$")
list(SORT nector_manifest_files)

set(nector_manifest_entries "")
foreach(relative_path IN LISTS nector_manifest_files)
    file(SIZE "${nector_package_root}/${relative_path}" file_size)
    file(SHA256 "${nector_package_root}/${relative_path}" file_hash)
    if(nector_manifest_entries)
        string(APPEND nector_manifest_entries ",\n")
    endif()
    string(APPEND nector_manifest_entries
        "    {\"path\":\"${relative_path}\",\"size\":${file_size},\"sha256\":\"${file_hash}\"}")
endforeach()

file(WRITE "${nector_package_root}/metadata/manifest.json"
    "{\n  \"name\":\"Nector\",\n  \"version\":\"0.1.0\",\n  \"abi_version\":0,\n  \"package_layout\":1,\n  \"files\":[\n${nector_manifest_entries}\n  ]\n}\n")

file(GLOB_RECURSE nector_checksum_files LIST_DIRECTORIES false
    RELATIVE "${nector_package_root}" "${nector_package_root}/*")
list(REMOVE_ITEM nector_checksum_files "metadata/checksums.sha256")
list(SORT nector_checksum_files)
set(nector_checksums "")
foreach(relative_path IN LISTS nector_checksum_files)
    file(SHA256 "${nector_package_root}/${relative_path}" file_hash)
    string(APPEND nector_checksums "${file_hash}  ${relative_path}\n")
endforeach()
file(WRITE "${nector_package_root}/metadata/checksums.sha256" "${nector_checksums}")
