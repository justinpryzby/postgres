# Tests for locking conflicts with CLUSTER command and partitions.

setup
{
	CREATE ROLE regress_cluster_part;
	CREATE TABLE cluster_part_tab (a int) PARTITION BY LIST (a);
	CREATE TABLE cluster_part_tab1 PARTITION OF cluster_part_tab FOR VALUES IN (1);
	CREATE TABLE cluster_part_tab2 PARTITION OF cluster_part_tab FOR VALUES IN (2);
	CREATE INDEX cluster_part_ind ON cluster_part_tab(a);
	ALTER TABLE cluster_part_tab OWNER TO regress_cluster_part;
	ALTER TABLE cluster_part_tab1 OWNER TO regress_cluster_part;
	ALTER TABLE cluster_part_tab2 OWNER TO regress_cluster_part;
}

teardown
{
	DROP TABLE cluster_part_tab;
	DROP ROLE regress_cluster_part;
}

session s1
step s1_begin          { BEGIN; }
step s1_lock_parent    { LOCK cluster_part_tab IN SHARE UPDATE EXCLUSIVE MODE; }
step s1_lock_child     { LOCK cluster_part_tab1 IN SHARE UPDATE EXCLUSIVE MODE; }
step s1_commit         { COMMIT; }

session s2
step s2_begin          { BEGIN; }
step s2_lock_child2    { LOCK cluster_part_tab2; }
step s2_owner_child1   { ALTER TABLE cluster_part_tab1 OWNER TO CURRENT_ROLE; }
step s2_commit         { COMMIT; }

session s3
step s3_auth           { SET ROLE regress_cluster_part; }
step s3_cluster        { CLUSTER cluster_part_tab USING cluster_part_ind; }
step s3_reset          { RESET ROLE; }

# CLUSTER on the parent waits if locked, passes for all cases.
permutation s1_begin s1_lock_parent s3_auth s3_cluster s1_commit s3_reset
permutation s1_begin s3_auth s1_lock_parent s3_cluster s1_commit s3_reset

# When a partition leaf changes ownership after CLUSTER begins, it's silently skipped
permutation s2_begin s2_lock_child2 s3_auth s3_cluster s2_owner_child1 s2_commit s3_reset
permutation s2_begin s2_lock_child2 s3_auth s3_cluster s2_owner_child1 s2_commit s3_reset
