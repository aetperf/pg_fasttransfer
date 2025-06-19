-- Safe test function for pg_fasttransfer extension
CREATE OR REPLACE FUNCTION pg_fasttransfer_safe()
RETURNS TABLE (
    exit_code integer, 
    output text,
    total_rows bigint,
    total_columns integer,
    transfer_time_ms bigint,
    total_time_ms bigint    
) AS 'pg_fasttransfer'
LANGUAGE C;