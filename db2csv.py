import sqlite3
import csv

file_path = "database.db"
conn = sqlite3.connect(file_path)
cur = conn.cursor()

cur.execute("SELECT name FROM sqlite_master WHERE type='table';")
tables = [t[0] for t in cur.fetchall()]

for table in tables:
    cur.execute(f"SELECT * FROM {table}")
    rows = cur.fetchall()
    headers = [desc[0] for desc in cur.description]

    with open(f"database/{table}.csv", "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(headers)
        writer.writerows(rows)

conn.close()
print("Export success")
