"""
HTTrack Website Scraper Actor
Main entry point for the Apify Actor
"""

import asyncio
from .main import main

if __name__ == '__main__':
    asyncio.run(main())
