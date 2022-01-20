ecpg_pgtypes_sources = files(
  'common.c',
  'datetime.c',
  'dt_common.c',
  'interval.c',
  'numeric.c',
  'timestamp.c',
)

ecpg_pgtypes = both_libraries('pgtypes',
  ecpg_pgtypes_sources,
  include_directories : ['.', ecpg_inc, postgres_inc],
  dependencies: [frontend_shlib_code],
  version: '3.'+pg_version_major.to_string(),
  soversion: 3,
  install: true,
)

pkgconfig.generate(
  ecpg_pgtypes.get_shared_lib(),
  name: 'libpgtypes',
  description: 'PostgreSQL libpgtypes library',
  url: pg_url,
)
