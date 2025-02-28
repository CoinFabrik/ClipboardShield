using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Threading.Tasks;

namespace hash_checker
{
    internal class Program
    {
        static int Main(string[] args)
        {
            bool ok = true;
            if (Hashes.HashesByPath.Length == 0)
            {
                Console.Error.WriteLine("Error: The hashes have not been generated. Please rebuild code_generation.");
                ok = false;
            }
            else
            {
                foreach (var (path, expected) in Hashes.HashesByPath)
                {
                    using (var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read))
                    {
                        var actual = BitConverter.ToString(SHA256.Create().ComputeHash(stream)).ToLower()
                            .Replace("-", "");
                        if (actual == expected)
                            continue;
                        Console.Error.WriteLine($"Error: {Path.GetFileName(path)} doesn't match the expected hash.");
                        ok = false;
                    }
                }
            }

            Console.WriteLine("Press ENTER to continue.");
            Console.ReadLine();

            return ok ? 0 : -1;
        }
    }
}
