-- BBC Sound Archive Database Initialization
-- Auto-executed by PostgreSQL on first container start

-- Enable vector extension for audio/video embeddings
-- Note: The extension is named 'vector', not 'pgvector'
CREATE EXTENSION IF NOT EXISTS vector;

-- Main sound effects table
CREATE TABLE bbc_sounds (
    id SERIAL PRIMARY KEY,
    
    -- CSV Data
    location TEXT UNIQUE NOT NULL,           -- "07067001.wav"
    description TEXT NOT NULL,               -- "Heavy rain"
    cdname TEXT NOT NULL,                    -- "Weather"
    cdnumber TEXT,                           -- "BBC Sound Effects No.1"
    category TEXT,                           -- "Weather: Rain"
    tracknum INTEGER,                        -- Track number on CD
    duration_seconds REAL NOT NULL,          -- Duration from CSV
    
    -- File Management
    file_path TEXT,                          -- "sounds/Weather/Heavy rain.07067001.wav"
    file_exists BOOLEAN DEFAULT FALSE,
    file_size_bytes BIGINT,                  -- Actual file size
    
    -- Embeddings (NULL until computed)
    audio_embedding vector(512),             -- Audio encoder embeddings
    video_embedding vector(768),             -- Video encoder embeddings (if used)
    text_embedding vector(512),              -- Text embeddings (CLIP/similar)
    
    -- Full-text Search (auto-generated from description + cdname)
    search_vector tsvector GENERATED ALWAYS AS (
        to_tsvector('english', description || ' ' || cdname)
    ) STORED,
    
    -- Metadata
    imported_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_accessed TIMESTAMP,
    access_count INTEGER DEFAULT 0
);

-- Indexes for performance
CREATE INDEX idx_cdname ON bbc_sounds(cdname);
CREATE INDEX idx_file_exists ON bbc_sounds(file_exists);
CREATE INDEX idx_duration ON bbc_sounds(duration_seconds);
CREATE INDEX idx_search ON bbc_sounds USING GIN(search_vector);

-- Vector indexes (create AFTER embeddings are populated)
-- Uncomment these after running embedding generation:
-- CREATE INDEX idx_audio_emb ON bbc_sounds USING ivfflat (audio_embedding vector_cosine_ops) WITH (lists = 100);
-- CREATE INDEX idx_video_emb ON bbc_sounds USING ivfflat (video_embedding vector_cosine_ops) WITH (lists = 100);
-- CREATE INDEX idx_text_emb ON bbc_sounds USING ivfflat (text_embedding vector_cosine_ops) WITH (lists = 100);

-- View for only available sounds (with existing files)
-- Use this for search, embedding generation, and API queries
CREATE VIEW available_sounds AS
SELECT * FROM bbc_sounds WHERE file_exists = TRUE;

-- Statistics view
CREATE VIEW sound_stats AS
SELECT 
    COUNT(*) as total_sounds,
    COUNT(*) FILTER (WHERE file_exists = TRUE) as available_sounds,
    COUNT(*) FILTER (WHERE file_exists = FALSE) as missing_sounds,
    SUM(duration_seconds) FILTER (WHERE file_exists = TRUE) / 3600.0 as total_hours_available,
    COUNT(DISTINCT cdname) as unique_categories,
    AVG(duration_seconds) as avg_duration_seconds
FROM bbc_sounds;

-- Category statistics
CREATE VIEW category_stats AS
SELECT 
    cdname,
    COUNT(*) as total_sounds,
    COUNT(*) FILTER (WHERE file_exists = TRUE) as available_sounds,
    SUM(duration_seconds) FILTER (WHERE file_exists = TRUE) as total_duration_seconds,
    AVG(duration_seconds) as avg_duration_seconds
FROM bbc_sounds
GROUP BY cdname
ORDER BY available_sounds DESC;

-- Grant permissions
GRANT ALL PRIVILEGES ON DATABASE bbc_sounds TO ludwig;
GRANT ALL PRIVILEGES ON ALL TABLES IN SCHEMA public TO ludwig;
GRANT ALL PRIVILEGES ON ALL SEQUENCES IN SCHEMA public TO ludwig;
GRANT SELECT ON available_sounds TO ludwig;

-- Log initialization
DO $$
BEGIN
    RAISE NOTICE 'BBC Sound Archive database initialized successfully';
    RAISE NOTICE 'pgvector extension enabled for embedding support';
    RAISE NOTICE 'Use sound_stats and category_stats views for statistics';
END $$;
