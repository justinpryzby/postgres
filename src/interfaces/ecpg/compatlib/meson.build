ecpg_compat = both_libraries('ecpg_compat',
   'informix.c',
   include_directories : ['.', ecpg_inc, postgres_inc, libpq_inc],
   dependencies: [frontend_shlib_code, thread_dep],
   link_with: [ecpglib, ecpg_pgtypes],
   soversion: 3,
   version: '3.'+pg_version_major.to_string(),
   install: true,
)

pkgconfig.generate(
  ecpg_compat.get_shared_lib(),
  name: 'libecpg_compat',
  description: 'PostgreSQL ecpg_compat library',
  url: pg_url,
)
